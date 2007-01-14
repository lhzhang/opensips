/* 
 * $Id$ 
 *
 * UNIXODBC module
 *
 * Copyright (C) 2005-2006 Marco Lorrai
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 */


#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../dprint.h"
#include "utils.h"
#include "val.h"

/*
 * add backslashes to special characters
 */
int sql_escape(char *dst, char *src, int src_len)
{
	int i, j;

	if(dst==0 || src==0 || src_len<=0)
		return 0;
	j = 0;
	for(i=0; i<src_len; i++)
	{
		switch(src[i])
		{
			case '\'':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '\\':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '\0':
				dst[j++] = '\\';
				dst[j++] = '0';
				break;
			default:
				dst[j++] = src[i];
		}
	}
	return j;
}
/*
 * remove backslashes to special characters
 */
int sql_unescape(char *dst, char *src, int src_len)
{
	int i, j;

	if(dst==0 || src==0 || src_len<=0)
		return 0;
	j = 0;
	i = 0;
	while(i<src_len)
	{
		if(src[i]=='\\' && i+1<src_len)
		{
			switch(src[i+1])
			{
				case '\'':
					dst[j++] = '\'';
					i++;
					break;
				case '\\':
					dst[j++] = '\\';
					i++;
					break;
				case '\0':
					dst[j++] = '\0';
					i++;
					break;
				default:
					dst[j++] = src[i];
			}
		} else {
			dst[j++] = src[i];
		}
		i++;
	}
	return j;
}


/*
 * Convert a string to integer
 */
static inline int str2int(const char* _s, int* _v)
{

	long tmp;

	if (!_s || !_v)
	{
		LOG(L_ERR, "str2int: Invalid parameter value\n");
		return -1;
	}

	tmp = strtoul(_s, 0, 10);
	if ((tmp == ULONG_MAX && errno == ERANGE) ||
		(tmp < INT_MIN) || (tmp > UINT_MAX))
	{
		LOG(L_ERR, "str2int: Value out of range\n");
		return -1;
	}

	*_v = (int)tmp;
	return 0;
}

/*
 * Convert a string to double
 */
static inline int str2double(const char* _s, double* _v)
{
	if ((!_s) || (!_v))
	{
		LOG(L_ERR, "str2double: Invalid parameter value\n");
		return -1;
	}

	*_v = atof(_s);
	return 0;
}

/* 
 * Convert a string to time_t
 */
static inline int str2time(const char* _s, time_t* _v)
{
	if ((!_s) || (!_v))
	{
		LOG(L_ERR, "str2time: Invalid parameter value\n");
		return -1;
	}

	*_v = odbc2time(_s);
	return 0;
}

/*
 * Convert an integer to string
 */
static inline int int2str(int _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l))
	{
		LOG(L_ERR, "int2str: Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-d", _v);
	if (ret < 0 || ret >= *_l)
	{
		LOG(L_ERR, "int2str: Error in sprintf\n");
		return -1;
	}
	*_l = ret;

	return 0;
}

/*
 * Convert a double to string
 */
static inline int double2str(double _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l))
	{
		LOG(L_ERR, "double2str: Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-10.2f", _v);
	if (ret < 0 || ret >= *_l)
	{
		LOG(L_ERR, "double2str: Error in snprintf\n");
		return -1;
	}
	*_l = ret;

	return 0;
}

/*
 * Convert time_t to string
 */
static inline int time2str(time_t _v, char* _s, int* _l)
{
	int l;

	if ((!_s) || (!_l) || (*_l < 2))
	{
		LOG(L_ERR, "time2str: Invalid parameter value\n");
		return -1;
	}

	*_s++ = '\'';
	l = time2odbc(_v, _s, *_l - 1);
	*(_s + l) = '\'';
	*_l = l + 2;
	return 0;
}

/*
 * Does not copy strings
 */
int str2val(db_type_t _t, db_val_t* _v, const char* _s, int _l)
{
	static str dummy_string = {"", 0};

	if (!_v)
	{
		LOG(L_ERR, "str2val: Invalid parameter value\n");
		return -1;
	}

	if (!_s || !strcmp(_s, "NULL"))
	{
		memset(_v, 0, sizeof(db_val_t));
/* Initialize the string pointers to a dummy empty
 * string so that we do not crash when the NULL flag
 * is set but the module does not check it properly
 */
		VAL_STRING(_v) = dummy_string.s;
		VAL_STR(_v) = dummy_string;
		VAL_BLOB(_v) = dummy_string;
		VAL_TYPE(_v) = _t;
		VAL_NULL(_v) = 1;
		return 0;
	}
	VAL_NULL(_v) = 0;

	switch(_t)
	{
		case DB_INT:
			if (str2int(_s, &VAL_INT(_v)) < 0)
			{
				LOG(L_ERR, "str2val: Error while converting integer value from string\n");
				return -2;
			}
			else
			{
				VAL_TYPE(_v) = DB_INT;
				return 0;
			}
			break;

		case DB_BITMAP:
			if (str2int(_s, &VAL_INT(_v)) < 0)
			{
				LOG(L_ERR, "str2val: Error while converting bitmap value from string\n");
				return -3;
			}
			else
			{
				VAL_TYPE(_v) = DB_BITMAP;
				return 0;
			}
			break;

		case DB_DOUBLE:
			if (str2double(_s, &VAL_DOUBLE(_v)) < 0)
			{
				LOG(L_ERR, "str2val: Error while converting double value from string\n");
				return -4;
			}
			else
			{
				VAL_TYPE(_v) = DB_DOUBLE;
				return 0;
			}
			break;

		case DB_STRING:
			VAL_STRING(_v) = _s;
			VAL_TYPE(_v) = DB_STRING;
			return 0;

		case DB_STR:
			VAL_STR(_v).s = (char*)_s;
			VAL_STR(_v).len = _l;
			VAL_TYPE(_v) = DB_STR;
			return 0;

		case DB_DATETIME:
			if (str2time(_s, &VAL_TIME(_v)) < 0)
			{
				LOG(L_ERR, "str2val: Error while converting datetime value from string\n");
				return -5;
			}
			else
			{
				VAL_TYPE(_v) = DB_DATETIME;
				return 0;
			}
			break;

		case DB_BLOB:
			VAL_BLOB(_v).s = (char*)_s;
			VAL_BLOB(_v).len = _l;
			VAL_TYPE(_v) = DB_BLOB;
			return 0;
	}
	return -6;
}

/*
 * Used when converting result from a query
 */
int val2str(SQLHDBC* _c, db_val_t* _v, char* _s, int* _len)
{
	int l;

	if (!_c || !_v || !_s || !_len || !*_len)
	{
		LOG(L_ERR, "val2str: Invalid parameter value\n");
		return -1;
	}

	if (VAL_NULL(_v))
	{
		if (*_len < sizeof("NULL"))
		{
			LOG(L_ERR, "val2str: Buffer too small\n");
			return -1;
		}
		*_len = snprintf(_s, *_len, "NULL");
		return 0;
	}

	switch(VAL_TYPE(_v))
	{
		case DB_INT:
			if (int2str(VAL_INT(_v), _s, _len) < 0)
			{
				LOG(L_ERR, "val2str: Error while converting string to int\n");
				return -2;
			}
			else
			{
				return 0;
			}
			break;

		case DB_BITMAP:
			if (int2str(VAL_BITMAP(_v), _s, _len) < 0)
			{
				LOG(L_ERR, "val2str: Error while converting string to int\n");
				return -3;
			}
			else
			{
				return 0;
			}
			break;

		case DB_DOUBLE:
			if (double2str(VAL_DOUBLE(_v), _s, _len) < 0)
			{
				LOG(L_ERR,
					"val2str: Error while converting string to double\n");
				return -4;
			}
			else
			{
				return 0;
			}
			break;

		case DB_STRING:
			l = strlen(VAL_STRING(_v));
			if (*_len < (l * 2 + 3))
			{
				LOG(L_ERR, "val2str: Destination buffer too short\n");
				return -5;
			}
			else
			{
				*_s++ = '\'';
				_s += sql_escape(_s, (char*)VAL_STRING(_v), l);
				*(_s + l) = '\'';
				*(_s + l + 1) = '\0';					   /* FIXME */
				*_len = l + 2;
				return 0;
			}
			break;

		case DB_STR:
			l = VAL_STR(_v).len;
			if (*_len < (l * 2 + 3))
			{
				LOG(L_ERR, "val2str: Destination buffer too short\n");
				return -6;
			}
			else
			{
				*_s++ = '\'';
				_s += sql_escape(_s, VAL_STR(_v).s, l);
				*(_s + l) = '\'';
				*(_s + l + 1) = '\0';					   /* FIXME */
				*_len = l + 2;
				return 0;
			}
			break;

		case DB_DATETIME:
			if (time2str(VAL_TIME(_v), _s, _len) < 0)
			{
				LOG(L_ERR, "val2str: Error while converting string to time_t\n");
				return -7;
			}
			else
			{
				return 0;
			}
			break;

		case DB_BLOB:
			l = VAL_BLOB(_v).len;
			if (*_len < (l * 2 + 3))
			{
				LOG(L_ERR, "val2str: Destination buffer too short\n");
				return -8;
			}
			else
			{
				*_s++ = '\'';
				_s += sql_escape(_s, VAL_BLOB(_v).s, l);
				*(_s + l) = '\'';
				*(_s + l + 1) = '\0';					   /* FIXME */
				*_len = l + 2;
				return 0;
			}
			break;

		default:
			DBG("val2str: Unknown data type\n");
			return -9;
	}
/*return -8; --not reached*/
}
