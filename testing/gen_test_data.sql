-- Used by testing/gen_test_data.sh to create testing data

CREATE DATABASE ybinlogp;
USE ybinlogp;
CREATE TABLE test1(x INT AUTO_INCREMENT PRIMARY KEY) ENGINE=InnoDB;
INSERT INTO test1 VALUES(1);
INSERT INTO test1 VALUES(2);
INSERT INTO test1 VALUES(3);
INSERT INTO test1 VALUES(4);
INSERT INTO test1 VALUES(5);
INSERT INTO test1 VALUES(6);
INSERT INTO test1 VALUES(7);

CREATE DATABASE foobar;
USE foobar;
CREATE TABLE test2(id INT AUTO_INCREMENT PRIMARY KEY, x VARCHAR(255)) ENGINE=InnoDB;
INSERT INTO test2(x) VALUES("This is the winter of our discontent");
INSERT INTO test2(x) VALUES("Tomorrow and tomorrow and tomorrow");
INSERT INTO test2(x) VALUES("Bananas r good");

FLUSH LOGS;

USE ybinlogp;

BEGIN;
    INSERT INTO test1 VALUES(8);
ROLLBACK;
BEGIN;
    INSERT INTO test1 VALUES(8);
COMMIT;
INSERT INTO test1 VALUES(9);

FLUSH LOGS;
