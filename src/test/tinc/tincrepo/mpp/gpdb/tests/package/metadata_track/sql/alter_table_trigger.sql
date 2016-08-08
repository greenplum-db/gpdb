--ENABLE & DISABLE TRIGGER

          CREATE TABLE mdt_price_change (
          apn CHARACTER(15) NOT NULL,
          effective TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
          price NUMERIC(9,2),
          UNIQUE (apn, effective)
          );

          CREATE TABLE mdt_stock(
          mdt_stock_apn CHARACTER(15) NOT NULL,
          mdt_stock_effective TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
          mdt_stock_price NUMERIC(9,2)
          )DISTRIBUTED RANDOMLY;


          CREATE TABLE mdt_stock1(
          mdt_stock_apn CHARACTER(15) NOT NULL,
          mdt_stock_effective TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
          mdt_stock_price NUMERIC(9,2)
          )DISTRIBUTED RANDOMLY;

          --trigger function to insert records as required:

          CREATE OR REPLACE FUNCTION mdt_insert_mdt_price_change() RETURNS trigger AS '
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

          --create a trigger on the table you wish to monitor:

          CREATE TRIGGER mdt_insert_mdt_price_change AFTER INSERT OR DELETE OR UPDATE ON mdt_stock
          FOR EACH ROW EXECUTE PROCEDURE mdt_insert_mdt_price_change();

          CREATE TRIGGER mdt_insert_mdt_price_change1 AFTER INSERT OR DELETE OR UPDATE ON mdt_stock1
          FOR EACH ROW EXECUTE PROCEDURE mdt_insert_mdt_price_change();


          ALTER TABLE mdt_stock DISABLE TRIGGER mdt_insert_mdt_price_change;
          ALTER TABLE mdt_stock1 ENABLE TRIGGER mdt_insert_mdt_price_change1;

drop table mdt_price_change;
drop table mdt_stock;
drop table mdt_stock1;
drop function mdt_insert_mdt_price_change();

