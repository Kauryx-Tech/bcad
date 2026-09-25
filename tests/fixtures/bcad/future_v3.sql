-- Fixture : un fichier .bcad d'une version que l'hôte ne connaît pas.
--
-- La forme est celle de la version 2 (type_id TEXT, entity_properties typée),
-- étiquetée user_version = 3, avec une table que personne n'écrit encore. Le
-- seul fait testé est le refus : un fichier plus récent que le chargeur n'est
-- pas lu avec un schéma qu'il ignore, et n'est pas réécrit.
--
-- Regenerer le binaire :
--     rm -f future_v3.bcad && sqlite3 future_v3.bcad < future_v3.sql

PRAGMA user_version = 3;

BEGIN TRANSACTION;
CREATE TABLE layers ( name TEXT PRIMARY KEY, color_r REAL, color_g REAL, color_b REAL,
                      line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER);
INSERT INTO layers VALUES('0',1.0,1.0,1.0,0.25,1,0,0);
CREATE TABLE entities ( id INTEGER PRIMARY KEY, type_id TEXT NOT NULL, layer TEXT,
                        has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL, params TEXT);
INSERT INTO entities VALUES(1,'bcad.Line','0',0,0.0,0.0,0.0,'0,0,5,5');
INSERT INTO entities VALUES(2,'cadastre.parcel','0',0,0.0,0.0,0.0,'1,0,0,6,0,6,4,0,4|A|001|1250 m2|Dakar|N-diaye|1');
CREATE TABLE entity_properties ( entity_id INTEGER NOT NULL, key TEXT NOT NULL, value_json TEXT NOT NULL,
                                 PRIMARY KEY (entity_id, key),
                                 FOREIGN KEY (entity_id) REFERENCES entities(id) ON DELETE CASCADE);
INSERT INTO entity_properties VALUES(2,'cadastre.contenance','{"type":"string","value":"1250 m2"}');
INSERT INTO entity_properties VALUES(2,'cadastre.surface','{"type":"double","value":1250.42}');
INSERT INTO entity_properties VALUES(2,'cadastre.nature','{"type":"enum","value":1,"values":["Bâtie","Non bâtie"]}');
CREATE TABLE entity_links ( src INTEGER, dst INTEGER, kind TEXT );
COMMIT;
