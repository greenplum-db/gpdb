-- @author rahmaf2
-- @tags MPP-25339 bfv FEATURE_BRANCH_ONLY
-- @product_version gpdb: [4.3.5-], [5.0-] hawq: [1.2.2.0-]
-- @Description Check if we create too many executor accounts
-- @vlimMB 200 

select count(distinct created_at) as fake from (select id, to_timestamp(creationdate) as created_at from minimal_mpp_25339) as test;
