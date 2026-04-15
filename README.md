# SQL Processor with B+Tree 

## 1. 인덱스란?
인덱스는 테이블의 검색 속도를 향상시키기 위해, 특정 컬럼의 값과 데이터 위치를 매핑하는 자료구조다.

## 2. 인덱스의 장점과 단점
## 장점
- 조회 속도 향상
- 특정 컬럼 조건 검색에 유리
- B+Tree의 경우 범위 검색에 강함
- 필요한 데이터만 빠르게 찾을 수 있다.

## 단점
- INSERT / UPDATE / DELETE가 느려질 수 있음
- 인덱스 저장공간이 추가로 필요함
- 데이터가 적거나, 너무 많은 행을 가져와야 하는 경우 오히려 비효율적일 수 있음


```text
5 -> 1 -> 9 -> 3 -> 7 -> 2
```

이런 삽입은 매번 다른 위치의 노드를 수정할 가능성이 높고, 디스크 기반 B+Tree에서는 디스크 I/O, page split, cache miss가 함께 발생할 수 있다.

---

## 3. B+Tree란?

![[Pasted image 20260415174302.png]]
PostgreSQL의 인덱스 구조

## 핵심 특징
1. 모든 실제 데이터는 리프 노드에 존재한다.
2. 내부 노드는 탐색용 키만 가진다.
3. 트리는 항상 균형을 유지한다.
4. 하나의 노드가 두 개 이상의 키를 가진다.
5. 리프 노드는 연결 리스트로 이어져 있다.

```text
[1, 2, 3] -> [4, 5, 6] -> [7, 8, 9]
```

리프 노드가 연결되어 있기 때문에 `BETWEEN`, `>=`, `<=` 같은 범위 검색에 강하다.

---

## 4. B+Tree 탐색 흐름

B+Tree에서 특정 키를 찾는 과정은 다음과 같다.

1. 루트 노드에서 시작한다.
2. 내부 노드의 키를 비교해서 내려갈 자식을 고른다.
3. 리프 노드에 도착한다.
4. 리프 노드에서 실제 데이터 위치를 찾는다.
5. 해당 위치의 row를 읽는다.

---

## 5. 왜 B+Tree는 DB에 적합한가?

B+Tree는 단순한 메모리 자료구조라기보다, 원래 디스크 페이지 접근을 줄이기 위해 설계된 자료구조다.

DB는 보통 디스크의 page 단위로 데이터를 읽는다.
그래서 B+Tree는 노드 하나에 여러 개의 키를 담아 fan-out을 크게 만든다.

fan-out이 크면 트리의 높이가 낮아진다.

```text
데이터 1,000,000개
fan-out 100
높이 약 4
```

즉, 100만 개의 데이터가 있어도 몇 번의 노드 접근만으로 원하는 위치 근처까지 갈 수 있다.

---

## 6. Page Split

B+Tree의 핵심 비용 중 하나는 page split이다.

노드가 꽉 찬 상태에서 중간에 새로운 키를 넣어야 하면, 기존 노드를 두 개로 나누고 부모 노드도 수정해야 한다.

```text
[1, 2, 3, 4]  <- 꽉 참

2.5 삽입

[1, 2] + [2.5, 3, 4]
```

이 split은 부모 노드로 전파될 수 있고, 최악의 경우 루트까지 변경될 수 있다.

그래서 B+Tree는 조회에는 강하지만, write-heavy한 상황에서는 인덱스 관리 비용이 커질 수 있다.

---

## 7. SQL Processor

우리 프로젝트는 C11로 작성한 파일 기반 SQL 처리기다.
이번 확장에서는 CSV 기반 SQL 처리기에 메모리 기반 B+Tree 인덱스 계층을 추가했다.

---
## 8. 실행 흐름

현재 실행 흐름은 다음과 같다.

```text
main.c
 ├─ CLI 인자 파싱
 ├─ SQL 파일 읽기
 ├─ Tokenizer (문자열 -> 토큰 분리)
 ├─ Parser (토큰 -> AST 변환)
 │
 ├─ DB Context 생성
 │   └─ db_context_create()
 │        ├─ schema/*.schema 스캔
 │        ├─ schema 메타데이터 로드
 │        ├─ tables/*.csv 데이터 로드
 │        ├─ RowSet 구성 (메모리 상 테이블)
 │        ├─ id 기준 B+Tree 인덱스 빌드
 │        └─ next_id = max(id) + 1 계산
 │
 ├─ 쿼리 실행
 │   └─ execute_query_list(queries, ctx)
 │        ├─ INSERT
 │        │    ├─ id 수동 입력 차단
 │        │    ├─ next_id로 id 자동 할당
 │        │    ├─ RowSet 갱신
 │        │    ├─ B+Tree 인덱스 갱신
 │        │    ├─ CSV 파일에 append
 │        │    └─ append 실패 시 rollback / index fallback 처리
 │        │
 │        └─ SELECT
 │             ├─ WHERE id = ? -> B+Tree 단건 조회
 │             ├─ WHERE id BETWEEN a AND b -> B+Tree 범위 조회
 │             ├─ 그 외 조건 -> RowSet 선형 탐색
 │             └─ ORDER BY가 있으면 결과 정렬
 │
 └─ DB Context 해제
      └─ db_context_destroy()

```


---
## 9. 프로젝트 핵심 구조
### 1. DbContext 도입
프로그램 실행 동안 테이블 상태를 메모리에 유지하기 위해 `DbContext`를 도입했다.

```c
typedef struct {
    char name[256];
    Schema schema;
    RowSet rowset;
    BPTree *index;
    int next_id;
} TableState;

typedef struct {
    char db_root[SQL_PATH_BUFFER_SIZE];
    TableState *tables;
    int table_count;
} DbContext;
```

`DbContext`는 다음 정보를 테이블별로 관리한다.

- schema 정보
- CSV에서 읽은 RowSet
- `id` 기준 B+Tree 인덱스
- 다음에 부여할 auto-increment id

## 2. B+Tree 인덱스
DbContext가 CSV 파일을 읽으면 먼저 RowSet이라는 CSV를 파싱한 데이터가 들어있는 배열이 만들어집니다.
```c
  rows[0] = ["1", "Alice", "3", "22", "Seoul", "4.1"]
  rows[1] = ["2", "Bob", "2", "20", "Busan", "3.7"]
  rows[2] = ["3", "Chris", "1", "19", "Incheon", "3.9"]
```

```c
B+Tree배열의 결과
1 -> 0
2 -> 1
3 -> 2

RowSet배열 접근
```

---

## 10. SELECT 최적화

`WHERE id = ?` 조건은 B+Tree 단건 검색을 사용한다.

```sql
SELECT name FROM users WHERE id = 10;
```

동작 흐름:

```text
B+Tree에서 id 10 검색
-> row_index 획득
-> RowSet[row_index] 접근
-> 결과 출력
```

기존에는 전체 row를 모두 비교해야 했지만, 지금은 트리 높이만큼만 내려가면 된다.

```text
기존: O(N)
변경: O(log_B N)
```

---

## 11. BETWEEN 범위 검색

이번 확장에서는 `BETWEEN` 조건도 추가했다.

```sql
SELECT id, name FROM users WHERE id BETWEEN 10 AND 20;
```

동작 흐름:

1. B+Tree에서 시작 key인 10이 들어갈 리프를 찾는다.
2. 리프 노드 안에서 10 이상인 위치부터 읽는다.
3. 리프의 `next` 포인터를 따라가며 20 이하의 key를 수집한다.
4. 수집한 row index로 RowSet에서 결과를 만든다.

그래서 시간 복잡도는 다음과 같다.

```text
O(log_B N + K)
```

여기서 `K`는 실제로 반환되는 row 수다.

---

## 12. 시간 복잡도 비교

기존 CSV 전체 스캔 방식:

```text
WHERE id = ?       O(N)
WHERE id BETWEEN   O(N)
INSERT 중복 검사   O(N)
```

B+Tree 적용 후:

```text
시작 시 인덱스 빌드       O(N log_B N)
WHERE id = ?              O(log_B N)
WHERE id BETWEEN a AND b  O(log_B N + K)
INSERT 인덱스 갱신        O(log_B N)
```

프로그램 시작 시 한 번 인덱스를 빌드하는 비용은 생기지만, 같은 세션에서 조회가 반복될수록 이득이 커진다.

---

## 13. 테스트한 핵심 케이스

검증한 핵심 동작은 다음과 같다.

- `WHERE id = existing`은 1건 반환
- `WHERE id = missing`은 0건 반환
- `WHERE id BETWEEN low AND high`는 범위 내 row 반환
- `WHERE id BETWEEN 5 AND 3`은 빈 결과 반환
- `INSERT` 시 `id` 직접 입력은 오류
- `INSERT` 시 `id` 생략은 자동 부여
- `ORDER BY score`는 float 기준 정렬
- CSV append 실패 시 메모리 row 오염 방지

---

## 14. 현재 한계

현재 구현은 의도적으로 범위를 좁혔다.

- 인덱스는 `id` 컬럼에만 존재한다.
- B+Tree는 메모리 전용이라 프로그램 시작 때마다 다시 빌드한다.
- 디스크 기반 B+Tree 페이지 구조는 아직 구현하지 않았다.
- `UPDATE`, `DELETE`는 아직 없다.
- 실행 가능한 benchmark CLI는 후속 작업으로 남아 있다.

즉, 현재 프로젝트는 실제 DBMS의 모든 기능을 구현한 것이 아니라, CSV 기반 SQL 처리기에 인덱스 계층을 붙였을 때 실행 경로가 어떻게 바뀌는지 보여주는 데 초점을 맞췄다.

---

## 15. 요약

이번 프로젝트에서 가장 중요한 변화는 세 가지다.

1. 매 쿼리마다 CSV 전체를 다시 읽던 구조에서, 시작 시 한 번 로드하고 세션 동안 유지하는 구조로 바뀌었다.
2. `id` 조건 검색은 선형 탐색 대신 B+Tree 인덱스를 사용하게 되었다.
3. auto-increment를 도입해 primary key를 시스템이 관리하도록 바꿨다.

결과적으로 우리 SQL Processor는 단순한 CSV reader에서 한 단계 나아가, DBMS의 핵심 아이디어인 "저장소와 인덱스의 분리"를 갖춘 구조가 되었다.

```text
CSV = 실제 데이터 저장소
RowSet = 세션 중 메모리 테이블
B+Tree = id 검색을 빠르게 하기 위한 인덱스
DbContext = 이 상태들을 묶어 관리하는 실행 컨텍스트
```

인덱스는 항상 빠른 마법이 아니라, 읽기 성능을 위해 쓰기 비용과 저장공간을 교환하는 구조다.
이번 구현은 그 trade-off를 직접 코드로 확인한 작업이다.
