create user mdt_user1 with superuser;
create group mdt_group1 with superuser;


          CREATE OR REPLACE FUNCTION mdt_insert_mdt_price_change1() RETURNS trigger AS '
          DECLARE
          changed boolean;
          BEGIN
          IF tg_op = ''DELETE'' THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (old.barcode, CURRENT_TIMESTAMP, NULL);
          RETURN old;
          END IF;
          IF tg_op = ''INSERT'' THEN
          changed := TRUE;
          ELSE
          changed := new.price IS NULL != old.price IS NULL OR new.price != old.price;
          END IF;
          IF changed THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (new.barcode, CURRENT_TIMESTAMP, new.price);
          END IF;
          RETURN new;
          END
          ' LANGUAGE plpgsql;

grant execute on function mdt_insert_mdt_price_change1() to mdt_user1 with grant option;


          CREATE OR REPLACE FUNCTION mdt_insert_mdt_price_change2() RETURNS trigger AS '
          DECLARE
          changed boolean;
          BEGIN
          IF tg_op = ''DELETE'' THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (old.barcode, CURRENT_TIMESTAMP, NULL);
          RETURN old;
          END IF;
          IF tg_op = ''INSERT'' THEN
          changed := TRUE;
          ELSE
          changed := new.price IS NULL != old.price IS NULL OR new.price != old.price;
          END IF;
          IF changed THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (new.barcode, CURRENT_TIMESTAMP, new.price);
          END IF;
          RETURN new;
          END
          ' LANGUAGE plpgsql;

grant execute on function mdt_insert_mdt_price_change2() to group mdt_group1 with grant option;


          CREATE OR REPLACE FUNCTION mdt_insert_mdt_price_change3() RETURNS trigger AS '
          DECLARE
          changed boolean;
          BEGIN
          IF tg_op = ''DELETE'' THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (old.barcode, CURRENT_TIMESTAMP, NULL);
          RETURN old;
          END IF;
          IF tg_op = ''INSERT'' THEN
          changed := TRUE;
          ELSE
          changed := new.price IS NULL != old.price IS NULL OR new.price != old.price;
          END IF;
          IF changed THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (new.barcode, CURRENT_TIMESTAMP, new.price);
          END IF;
          RETURN new;
          END
          ' LANGUAGE plpgsql;

grant all on function mdt_insert_mdt_price_change3() to public;


          CREATE OR REPLACE FUNCTION mdt_insert_mdt_price_change4() RETURNS trigger AS '
          DECLARE
          changed boolean;
          BEGIN
          IF tg_op = ''DELETE'' THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (old.barcode, CURRENT_TIMESTAMP, NULL);
          RETURN old;
          END IF;
          IF tg_op = ''INSERT'' THEN
          changed := TRUE;
          ELSE
          changed := new.price IS NULL != old.price IS NULL OR new.price != old.price;
          END IF;
          IF changed THEN
          INSERT INTO mdt_price_change(apn, effective, price)
          VALUES (new.barcode, CURRENT_TIMESTAMP, new.price);
          END IF;
          RETURN new;
          END
          ' LANGUAGE plpgsql;

grant all privileges on function mdt_insert_mdt_price_change4() to public;

drop function mdt_insert_mdt_price_change1();
drop function mdt_insert_mdt_price_change2();
drop function mdt_insert_mdt_price_change3();
drop function mdt_insert_mdt_price_change4();
drop user mdt_user1;
drop group mdt_group1;


