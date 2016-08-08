----------------------------------------------------------------------------------------------------------------------
--1) PL/R Demo
----------------------------------------------------------------------------------------------------------------------

----------------------------------------------------------------------------------
--					ETL				          --
----------------------------------------------------------------------------------	


	set search_path = gpdemo, public, pg_catalog;


	drop table if exists gpdemo.patient_history_train cascade;
	create table gpdemo.patient_history_train as (
	     select * from gpdemo.patient_history_all where TEMP_FOR_SPLITTING < 0.20
	);

	drop table if exists gpdemo.patient_history_test cascade;
	create table gpdemo.patient_history_test as (
		select * from gpdemo.patient_history_all where TEMP_FOR_SPLITTING > 0.20
	);



----------------------------------------------------------------------------------
--					ANALYTICS				  --
----------------------------------------------------------------------------------	

--1) Use case :  PL/R model save, load and score example

	-- i) Define a data type to hold the result from the GLM model. You may also define this as a table instead.    
	DROP TYPE IF EXISTS gpdemo.glm_result_type CASCADE;
	CREATE TYPE gpdemo.glm_result_type 
        AS 
	(
		params text, 
		estimate float, 
		std_Error float, 
		z_value float, 
		pr_gr_z float
        );

	drop language if exists 'plr' cascade;	
	create language 'plr';
	

	DROP FUNCTION IF EXISTS gpdemo.mdl_save_demo();
	CREATE FUNCTION gpdemo.mdl_save_demo() 
        RETURNS bytea 
        AS
	$$
	     #Read the previously created patient_history training set
	     dataset <- pg.spi.exec('select * from gpdemo.patient_history_train');

	     # Use the subset function to select a subset of the columns
             # Indices 2:6 are age, gender, race, marital status and bmi
             # Indices 14:20 are med_cond1 to med_cond7
             # Index 26 is the label 'infection cost'
	     ds = subset(dataset,select=c(2:6,14:20, 26))

	     #Define text  columns to be factor types
	     #These include gender, race, marital_status	     
	     ds$gender = as.factor(ds$gender)
	     ds$race = as.factor(ds$race)
	     ds$marital_status = as.factor(ds$marital_status)

	     #Fit a GLM
	     mdl = glm(formula = infection_cost ~ age +
				  gender +
				  race +
				  marital_status +
				  bmi +
				  med_cond1 +
				  med_cond2 +
				  med_cond3 +
				  med_cond4 +
				  med_cond5 +
				  med_cond6 +
				  med_cond7 
			, family = gaussian, data=ds)

             #Remove the data from the model (we only want to store the model, not the training set
             #mdl$data = NULL
             #mdl$qr = qr(qr.R(mdl$qr))
	     #The model is serialized and returned as a bytearray
	     return (serialize(mdl,NULL))
	$$
	LANGUAGE 'plr';

	select 'mdl_binary_size' as param
	from
	(
		select gpdemo.mdl_save_demo() as mdl
	)q;

	--4) Create a table to store some R models (ex: the GLM model).
	DROP TABLE IF EXISTS gpdemo.plr_mdls CASCADE;
	CREATE TABLE gpdemo.plr_mdls (id bigint, model bytea);


	--5) Insert a row in the mdls table, with an integer for the ID column and the serialized GLM model for the model column.
	insert into gpdemo.plr_mdls 
	values(
			(select count(*)+1 from gpdemo.plr_mdls), (select gpdemo.mdl_save_demo() as mdl)
	) ;



	DROP FUNCTION IF EXISTS gpdemo.mdl_load_demo(bytea);
	CREATE FUNCTION gpdemo.mdl_load_demo(mdl bytea) 
        RETURNS setof gpdemo.glm_result_type 
        AS
	$$
	     #R-code goes here.
	     mdl <- unserialize(mdl)
	     cf <- coef(summary(mdl))
	     rows = dimnames(cf)[1]
	     #Create a data frame and pass that as a result
	     result = data.frame(params=rows[[1]],estimate=cf[,1],error=cf[,2],z_val=cf[,3],pr_z=cf[,4])
	     return (result)
	$$
	LANGUAGE 'plr';

	--6) Obtain the coefficients of each model saved in the mdls table. 
	-- We can retrieve each column in the result of the subquery 'q' which returns rows of type glm_result_type defined in step 3.
	select count((t).params)
	from 
	(
	       -- The column 't' is of glm_result_type that we defined in step 3s.
	       select gpdemo.mdl_load_demo(model) as t 
	       from gpdemo.plr_mdls
	) q ;

	--7) Define a function to demonstrate Scoring. We'll load a  previously saved GLM model and use it for scoring
	DROP FUNCTION IF EXISTS gpdemo.mdl_score_demo( bytea, 
							integer,
							text,
							text,
							text,
							double precision,
							integer,
							integer,
							integer,
							integer,
							integer,
							integer,
							integer
						      );
	CREATE FUNCTION gpdemo.mdl_score_demo( mdl bytea, 
						age integer,
						gender text,
						race text,
						marital_status text,
						bmi double precision,
						med_cond1 integer,
						med_cond2 integer,
						med_cond3 integer,
						med_cond4 integer,
						med_cond5 integer,
						med_cond6 integer,
						med_cond7 integer	
					      ) 
	RETURNS numeric AS
	$$
	     if (pg.state.firstpass == TRUE) {
	     	#Unserialize the model (i.e reconstruct it from its binary form).
	        assign("gp_plr_mdl_score", unserialize(mdl) ,env=.GlobalEnv)
	        assign("pg.state.firstpass", FALSE, env=.GlobalEnv)
	     }

	     
	     #Read the test set from the previously created table  
	     test_set <- data.frame(
					age = age,
					gender = gender,
					race = race,
					marital_status = marital_status, 
					bmi =  bmi,
					med_cond1 =  med_cond1,
					med_cond2 =  med_cond2,
					med_cond3 =  med_cond3,
					med_cond4 =  med_cond4,
					med_cond5 =  med_cond5,
					med_cond6 =  med_cond6,
					med_cond7 =  med_cond7  	
			            );
	     #Perform prediction
	     pred <- predict(gp_plr_mdl_score, newdata=test_set, type="response"); 
	     
	     return (pred)
	$$
	LANGUAGE 'plr';

	--8) Score the test set using the scoring function defined in step 7.

	-- Compute R square (coefficient of determination)
	-- R_square = (1 - SS_err/SS_tot)
	select 'PL/R glm model '::text as model, round(sum(ss_err)) + NULL
	from
	(
		select instance_num, 
		(infection_cost_actual - (select avg(infection_cost) from gpdemo.patient_history_test) )^2.0 as ss_tot,
		(infection_cost_actual -  infection_cost_predicted)^2.0 as ss_err,		
		1 as cnt
		from
		(
			-- Show actual vs predicted values for the infection cost
			select row_number() over (order by random()) as instance_num, 
				infection_cost as infection_cost_actual,
				gpdemo.mdl_score_demo ( mdls.model, 
							age,
							gender,
							race,
							marital_status,
							bmi,
							med_cond1,
							med_cond2,
							med_cond3,
							med_cond4,
							med_cond5,
							med_cond6,
							med_cond7		
						      ) as infection_cost_predicted 
			from gpdemo.plr_mdls mdls, gpdemo.patient_history_test test limit 10
		) q1
	) q2 group by cnt;

	-- Can be speed up faster by order of magnitude by passing sets of rows to the scoring function
	

----------------------------------------------------------------------------------------------------------------------
