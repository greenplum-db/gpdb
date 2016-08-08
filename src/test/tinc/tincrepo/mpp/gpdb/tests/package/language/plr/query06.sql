drop schema if exists rtest cascade;
create schema rtest;
set search_path=rtest, public;

drop language if exists 'plr' cascade;
create language 'plr';

drop function if exists install_rcmd(text);

CREATE OR REPLACE FUNCTION install_rcmd (text)
RETURNS text
AS '$libdir/plr','install_rcmd'
LANGUAGE 'C' WITH (isstrict);

create or replace function my_r_primes() returns integer as '
    	x <- 1:100
	y <- rep(T,100)
	for (i in 3:100) {
    	if (all(i%%(2:(i-1))!=0)){
        	y <- TRUE
        	} else {y <- FALSE
                	}
	}
	print(x[y])
	return(0)
' language 'plr';

select my_r_primes();


select install_rcmd('craps <- function() {
        #returns TRUE if you win, FALSE otherwise
        initial.roll <- sum(sample(1:6,2,replace=T))
        if (initial.roll == 7 || initial.roll == 11) return(TRUE)
        while (TRUE) {
                current.roll <- sum(sample(1:6,2,replace=T))
                if (current.roll == 7 || current.roll == 11) {
                return(FALSE)
                } else if (current.roll == initial.roll) {
                return(TRUE)
                }
        }
        }');

create or replace function my_r_gamewin() returns integer as '
	mean(replicate(10000, craps()))
	print(mean)
	return(0)
' language 'plr';

select my_r_gamewin();