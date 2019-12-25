#ifndef __ODBC_PRIVATE_H__
#define __ODBC_PRIVATE_H__

#include <sqltypes.h>
#include <sql.h>
#include <sqlext.h>
#include "iSQL.h"
#include "iMemory.h"
#include "iRepository.h"
#include "iLog.h"

struct sql: public _sql_t {
private:
	SQLHDBC		m_hdbc;
	SQLHSTMT	m_hstmt;

public:
	sql() {
		m_hstmt = NULL;
	}

	bool _init(SQLHDBC hdbc);
	void _free(void);
	void _destroy(void);
};

typedef struct dbc _dbc_t;

struct dbc {
private:
	SQLHENV		m_henv;
	SQLHDBC		m_hdbc;
	SQLUSMALLINT	m_stmt_limit;
	SQLUSMALLINT	m_stmt_count;
	iPool		*mpi_stmt_pool;
	iLog		*mpi_log;
public:
	dbc() {
		m_henv = NULL;
		m_hdbc = NULL;
		m_stmt_limit = 0;
		m_stmt_count = 0;
		mpi_stmt_pool = 0;
		mpi_log = 0;
	}

	bool init(_cstr_t connect_string);
	void destroy(void);
};

#endif
