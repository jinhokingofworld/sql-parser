INSERT INTO users (name, grade, age, region, score) VALUES ('Alice', 2, 20, 'Seoul', 3.80);
INSERT INTO users (name, grade, age, region, score) VALUES ('Bob', 4, 25, 'Busan', 4.10);
INSERT INTO users (name, grade, age, region, score) VALUES ('Charlie', 1, 19, 'Incheon', 3.25);
SELECT id, name, score FROM users ORDER BY score DESC;
