-- Fixture : un fichier .bcad tel que l'ecrivait la version 1 du format natif.
--
-- Ce n'est pas un dump d'un fichier reel (les seuls fichiers v1 du depot sont
-- des produits de test), c'est la reproduction at-croquis du schema et des
-- valeurs que src/io/Database.cpp ecrivait avant la table generale
-- `entity_properties` : `PRAGMA user_version = 1`, `entities.type` entier, et
-- les six colonnes metiers de `cadastre_parcels`.
--
-- Regenerer le binaire :
--     rm -f legacy_v1.bcad && sqlite3 legacy_v1.bcad < legacy_v1.sql
--
-- Chaque ligne est choisie pour un cas de lecture, pas pour ressembler a un
-- dessin :
--   1  type=5 sans '|'  : une polyligne enrichie, jamais promue — le seul cas
--      que l'ecrivain v1 produisait vraiment. La table porte les valeurs.
--      `proprietaire` contient guillemet et barre oblique inverse : c'est la
--      valeur d'evasion JSON du dump.
--   2  type=5 avec '|'  : le format parcelle, ou les valeurs voyagent dans
--      `params` ET dans la table, en desaccord (contenance). La table gagne.
--   3  type=1 + couleur explicite.
--   4  type=6 texte.
--   5  type=5 sans '|' et sans ligne dans la table : geometry seule.
--   6  type=9 : un entier que le format v1 lui-meme ne sait pas nommer.

PRAGMA user_version = 1;

BEGIN TRANSACTION;
CREATE TABLE layers ( name TEXT PRIMARY KEY, color_r REAL, color_g REAL, color_b REAL,
                      line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER);
INSERT INTO layers VALUES('0',1.0,1.0,1.0,0.25,1,0,0);
INSERT INTO layers VALUES('CADASTRE',0.8,0.2,0.2,0.35,1,0,0);
CREATE TABLE entities ( id INTEGER PRIMARY KEY, type INTEGER, layer TEXT,
                        has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL, params TEXT);
INSERT INTO entities VALUES(1,5,'CADASTRE',0,0.0,0.0,0.0,'1,0,0,20,0,20,10,0,10');
INSERT INTO entities VALUES(2,5,'CADASTRE',0,0.0,0.0,0.0,'1,0,0,30,0,30,15,0,15|A|012|999 m2|Dakar|TRAORE Awa|2');
INSERT INTO entities VALUES(3,1,'0',1,0.2,0.4,0.6,'0,0,5,5');
INSERT INTO entities VALUES(4,6,'0',0,0.0,0.0,0.0,'1,2,0.5,0,BONJOUR');
INSERT INTO entities VALUES(5,5,'CADASTRE',0,0.0,0.0,0.0,'1,0,0,5,0,5,5,0,5');
INSERT INTO entities VALUES(6,9,'0',0,0.0,0.0,0.0,'7,8');
CREATE TABLE cadastre_parcels ( entity_id INTEGER PRIMARY KEY, section TEXT, numero TEXT,
                                contenance TEXT, commune TEXT, proprietaire TEXT, nature TEXT,
                                FOREIGN KEY(entity_id) REFERENCES entities(id));
INSERT INTO cadastre_parcels VALUES(1,'AB','0072','1234 m2','Abidjan','KOUASSI "K." \ Kofi','');
INSERT INTO cadastre_parcels VALUES(2,'A','012','1250 m2','Dakar','','');
COMMIT;
