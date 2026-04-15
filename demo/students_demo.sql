SELECT id, name, score FROM students WHERE id = 20250002;
SELECT id, name, region FROM students WHERE region = 'Seoul';
SELECT id, name, grade FROM students WHERE id BETWEEN 20250001 AND 20250003 ORDER BY id;
INSERT INTO students (id, name, grade, age, region, score)
VALUES (20250004, 'Choi Yuna', 4, 23, 'Daejeon', 4.50);
SELECT id, name, score FROM students WHERE id = 20250004;
