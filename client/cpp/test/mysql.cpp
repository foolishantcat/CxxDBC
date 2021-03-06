/*
 * mysql.cpp
 *
 */

#include "Edb.hh"

#define LOG(fmt,...) ESystem::out->printfln(fmt, ##__VA_ARGS__)

#define HOST "localhost"
#define PORT "6633"
#define DATABASE "mysql"
#define USERNAME "mysql"
#define PASSWORD "password"

static void test_db_connect() {
	EConnection conn;

	//1.
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);
	ON_FINALLY_NOTHROW(
		conn.close();
	) {
		EDatabaseMetaData* meta = conn.getMetaData();
		LOG("name=%s", meta->getDatabaseProductName().c_str());
		LOG("version=%s", meta->getDatabaseProductVersion().c_str());
	}}

	//2.
	conn.connect("edbc:MYSQL://" HOST ":" PORT "/" DATABASE "?connectTimeout=10", USERNAME, PASSWORD);
	ON_FINALLY_NOTHROW(
		conn.close();
	) {
		EDatabaseMetaData* meta = conn.getMetaData();
		LOG("name=%s", meta->getDatabaseProductName().c_str());
		LOG("version=%s", meta->getDatabaseProductVersion().c_str());
	}}
}

static void test_db_execute() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	EResultSet* rs;

	//0.
	try {
		stmt.setSql("DROP TABLE mysql000").execute();
	} catch (...) {
	}
	stmt.setSql("CREATE TABLE mysql000 ("
				  "id integer NULL,"
				  "name varchar (40) NULL ,"
				  "date date NULL"
				  ") type=InnoDB").execute();

	//1.
	stmt.clear();
	stmt.setSql("insert into mysql000 values(?,?,?)")
				.bindInt(4)
				.bindString("1")
				.bindString("2017-07-08");
	for (int i=0; i<100; i++) {
		stmt.execute();
	}

	//2.
	stmt.clear();
	stmt.setSql("select * from mysql000").execute();
	rs = stmt.getResultSet();
	EResultSetMetaData* rsm = rs->getMetaData();
	LOG(rsm->toString().c_str());
	while (rs->next()) {
		LOG("%d", rs->getInt(1));
		LOG("%s", rs->getBoolean(2) ? "true" : "false");
		LOG("[%s]", rs->getString(2).c_str());
		LOG("%s", rs->getDate("date").toString("%Y-%m-%d").c_str());
	}
	rs->close();

	//3.
	stmt.clear();
	stmt.setFetchSize(2);
	stmt.setSql("select * from mysql000 where id=? or name=?")
			.bindInt(4)
			.bindString("1")
			.execute();
	rs = stmt.getResultSet();
	while (rs != null) {
		while (rs->next()) {
			LOG("%d", rs->getInt(1));
			LOG("%s", rs->getBoolean(2) ? "true" : "false");
			LOG("[%s]", rs->getString(2).c_str());
			LOG("%s", rs->getDate("date").toString("%Y-%m-%d").c_str());
		}

		rs = stmt.getResultSet(true);
	}
	if (rs != null) rs->close();

	conn.close();
}

static void test_db_update() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);
	stmt.setFailedResume(true);

	stmt.setSql("insert into mysql000 values (-2, '666', '2015-5-5')"); //1
	stmt.addBatch("insert into mysql000 values (?, ?, ?)"); //2
	stmt.bindInt(-1);
	stmt.bindString("777");
	stmt.bindString("2016-6-6");
	stmt.addBatch("insert into mysql000 values (?, ?, ?)"); //3
		stmt.bindInt(-3);
		stmt.bindString("333");
		stmt.bindString("2016-6-6");
	for (int i = 0; i < 99; i++) {
		stmt.addBatch();
		if (i == 0) { //模拟错误
			stmt.bindInt(i);
		} else {
			stmt.bindInt(i);
			stmt.bindString("999");
			stmt.bindString("2016-6-6");
		}
	}
	stmt.addBatch("insert into mysql000 values (-2, '666', '2015-5-5')"); //103
	stmt.addBatch("insert into mysql000 values (-2, '666', '2015-5-5')"); //104
	stmt.execute();

	LOG("failures=%d", stmt.getFailures());
	LOG("first failed=%d", stmt.getFirstFailed());

	LOG("sql 1 affected=%d", stmt.getUpdateCount(1));
	LOG("sql 3 affected=%d", stmt.getUpdateCount(3));
	LOG("sql 3 errmsg=%s", stmt.getUpdateMessage(3).c_str());
	LOG("sql 4 affected=%d", stmt.getUpdateCount(4));
	LOG("sql 4 errmsg=%s", stmt.getUpdateMessage(4).c_str());
	LOG("sql 5 affected=%d", stmt.getUpdateCount(5));
	LOG("sql 6 affected=%d", stmt.getUpdateCount(6));
	LOG("sql 103 affected=%d", stmt.getUpdateCount(103));
	LOG("sql 104 affected=%d", stmt.getUpdateCount(104));

	conn.close();
}

static void test_db_commit() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	stmt.setSql("update mysql000 set name='xxxxxxxxxx' where id=1");
	conn.setAutoCommit(false);
	stmt.execute();
//	conn.commit();
	conn.rollback();
//	conn.setAutoCommit(true);
	stmt.execute();
	conn.commit();

	conn.close();
}

static void test_db_rollback() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	try {
		stmt.setSql("drop table mysql002").execute();
	} catch (...) {
	}
	stmt.setSql("create table mysql002 (a int) type=InnoDB").execute();
	conn.setAutoCommit(false);
	ESavepoint sp1 = conn.setSavepoint();
	stmt.setSql("insert into mysql002 values (1)").execute();
//	conn.rollback();
//	conn.setAutoCommit(true);
	ESavepoint sp2 = conn.setSavepoint();
	stmt.setSql("insert into mysql002 values (2)").execute();
	conn.rollback(sp2);
	conn.commit();

	conn.close();
}

static void test_large_object() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	conn.setAutoCommit(false);

	llong oid = conn.createLargeObject();
	LOG("create oid=%ld", oid);

	char *s = "12345678905567894598765679";
//	EByteArrayInputStream bais(s, strlen(s));
	EByteArrayOutputStream baos;
	for (int i = 0; i< 10000000; i++) {
		baos.write(s);
	}
	EByteArrayInputStream bais(baos.data(), baos.size());
	llong written = conn.writeLargeObject(oid, &bais);
	LOG("written=%ld", written);

	EFileOutputStream fos("/tmp/lob.txt");
	llong reading = conn.readLargeObject(oid, &fos);
	LOG("reading=%ld", reading);

	conn.commit();

	conn.close();
}

static void test_create_table() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);
	stmt.setFailedResume(true);
	stmt.addBatch("drop table mysql001");
	stmt.addBatch("CREATE TABLE mysql001 ("
            "a int(11) NULL ,"
            "b tinyint NULL ,"
            "c binary (50) NULL ,"
            "d bit(4) NULL ,"
            "e longtext NULL,"
            "f datetime NULL ,"
            "g float NULL ,"
            "h decimal(12,4) NULL ,"
            "i char (255) NULL ,"
            "j varchar (255) NULL ,"
            "k bigint NULL ,"
            "l LONGBLOB NULL ,"
            "m int(2) NULL ,"
            "n int(4) NULL ,"
            "o bool NULL ,"
            "p date NULL ,"
            "q timestamp NULL "
            ") Engine=InnoDB DEFAULT CHARSET=gb2312");
	stmt.execute();

	conn.close();
}

static void test_sql_insert() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	//large lob
	ECommonStatement st(&conn);
	st.setSql("insert into mysql001 (d,e,l) values (1,?,?)");
	EFileInputStream fis1("/tmp/a.csv");
	EFileInputStream fis2("/tmp/b.csv");
	st.bindAsciiStream(&fis1);
	st.bindBinaryStream(&fis2);
	st.execute();

	//other
	EUpdateStatement stmt(&conn);
	stmt.addBatch("insert into mysql001 (a,b,c,d,e,f,g,h,i,j,k,l) "
	"values (100000,20,0x616161616161,1,'中午的太阳火辣辣','2005-01-09 23:12:59',21212.4343,3333.0001,"
	"'舞起来','varchar类型',9223372036854775807,0x61616161616122)");
	stmt.addBatch("insert into mysql001 (d,e,g) values (1,?,?)");
	es_buffer_t *buffer = eso_buffer_make(100, 0);
	for (int i = 0; i < 2; i++) {
		eso_buffer_append(buffer, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
			50);
	}
	stmt.bind(DB_dtCLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);
	stmt.bindFloat(5.44454);
	stmt.addBatch("insert into mysql001 (a,b,c,d,e,f,i,p,q) values (?,?,?,0,?,?,?,?,?)");
	stmt.bindInt(55555);
	stmt.bindShort(12);
	stmt.bindBytes((byte *)"\1\2\3\4\0\9\8", 7);
	stmt.bindString("计费地热 热");
	stmt.bindDateTime("2005-01-09 23:12:59");
	stmt.bindString("2222031");
	stmt.bindDateTime("2005-01-09 23:12:59");
	stmt.bindDateTime("2005-01-09 23:12:59");

	stmt.setFailedResume(true);
	stmt.execute();
	LOG("err=%s, first failed sql index=%d, failures=%d", stmt.getErrorMessage().c_str(), stmt.getFirstFailed(), stmt.getFailures());
	for (int i=1; i <= stmt.getSqlCount(); i++) {
		LOG("sql#%d affected=%d, errmsg=%s", i, stmt.getUpdateCount(i), stmt.getUpdateMessage(i).c_str());
	}

	conn.close();
}

static void test_sql_query() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	ECommonStatement stmt(&conn);
	stmt.setSql("select * from mysql001").execute();
	EResultSet* rs = stmt.getResultSet();
	while (rs->next()) {
		for (int i=1; i<=rs->getMetaData()->getColumnCount(); i++) {
			sp<EInputStream> is = rs->getBinaryStream(i);
			LOG("%s:%d", rs->getMetaData()->getColumnLabel(i).c_str(), is->available());
		}
	}

	conn.close();
}

static void test_sql_update() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EUpdateStatement stmt(&conn);

	//1.
	stmt.setSql("update mysql001 set k=?,m=?,n=?,o=?,g=?,h=?");
	stmt.bindLong(ELLong::MAX_VALUE);
	stmt.bindShort(EShort::MAX_VALUE);
	stmt.bindInt(EInteger::MAX_VALUE);
	stmt.bindBool(1);
	stmt.bindFloat(8.8843);
	stmt.bindNumeric("55555.5454");
	stmt.execute();

	//2. text: <=64k
	stmt.addBatch("update mysql001 set e=?");
	es_buffer_t *buffer = eso_buffer_make(100, 0);
	for (int i = 0; i < 1000; i++) {
		eso_buffer_append(buffer, "a中午的太aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
			50);
	}
	stmt.bind(DB_dtCLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);

	//3. image: <=64k
	stmt.addBatch("update mysql001 set l=?");
	buffer = eso_buffer_make(100, 0);
	for (int i = 0; i < 10; i++) {
		eso_buffer_append(buffer, "\0\1\2\aaaaaaaaa中午的太aaaaaaaaaaaaaaaaaaaaaaaaaaa11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
			50);
	}
	stmt.bind(DB_dtCLob, (char*)buffer->data, buffer->len);
	eso_buffer_free(&buffer);

	stmt.execute();

	LOG("sql 1 effected = %d", stmt.getUpdateCount(1));
	LOG("sql 2 effected = %d", stmt.getUpdateCount(2));
	LOG("sql 3 effected = %d", stmt.getUpdateCount(3));

	conn.close();
}

static void test_sql_func() {
	EConnection conn;
	conn.connect(DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	//create
	{
		EUpdateStatement stmt(&conn);
		stmt.setFailedResume(true);
		stmt.addBatch("drop procedure mysql001func");
		stmt.addBatch("create procedure mysql001func (param1 int) "
                "begin "
                "select * from mysql001 where a=param1; "
                "select a,b,c,e,f from mysql001; "
                "end");
		stmt.execute();
	}

	//select
	{
		ECommonStatement stmt(&conn);
		stmt.setSql("call mysql001func(55555)");
//		stmt.bindInt(55555);
		stmt.execute();
		EResultSet* rs = stmt.getResultSet();
		while (rs != null) {
			while (rs->next()) {
				for (int i=1; i<=rs->getMetaData()->getColumnCount(); i++) {
					LOG("%s:%s", rs->getMetaData()->getColumnLabel(i).c_str(), rs->isNull(i) ? "is null" : rs->getString(i).c_str());
				}
			}
			rs = stmt.getResultSet(true);
		}
		if (rs != null) rs->close();
	}

	conn.close();
}

static void test_connect_pool() {
	EConnectionPool pool(10, NULL, DATABASE, HOST, atoi(PORT), USERNAME, PASSWORD);

	EArrayList<sp<EThread> > threads;
	for (int i = 0; i < 20; i++) {
		sp<EThread> ths = EThread::executeX([&]() {
			sp<EConnection> conn;
			for (int j = 0; j < 1000; j++) {
				conn = pool.getConnection();
				ES_ASSERT (conn != null);
				sp<ECommonStatement> stmt = conn->createCommonStatement();
				stmt->setSql("update mysql002 set a=1").execute();

				EThread::sleep(100);

				conn->close();
			}
		});
		threads.add(ths);
	}

	for (int i=0; i<threads.size(); i++) {
		threads[i]->join();
	}

	pool.close();
}

//=============================================================================

void test_db_mysql(void)
{
	EConnection::setDefaultDBType("MYSQL");

//	test_db_connect();
//	test_db_execute();
//	test_db_update();
//	test_db_commit();
//	test_db_rollback();
////	test_large_object();
//	test_create_table();
//	test_sql_insert();
	test_sql_query();
//	test_sql_update();
//	test_sql_func();
//	test_connect_pool();
}
