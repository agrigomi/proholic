#ifndef __JSON_H__
#define __JSON_H__

#include "context.h"

#define JSON_OK			0
#define JSON_PARSE_ERROR	-1
#define JSON_MEMORY_ERROR	-2

/* JSON value types */
#define JSON_STRING	1
#define JSON_NUMBER	2
#define JSON_OBJECT	3
#define JSON_ARRAY	4
#define JSON_TRUE	5
#define JSON_FALSE	6
#define JSON_NULL	7

typedef struct json_string 	_json_string_t;
typedef struct json_value	_json_value_t;
typedef struct json_array	_json_array_t;
typedef struct json_string	_json_number_t;
typedef struct json_pair	_json_pair_t;
typedef struct json_object	_json_object_t;

typedef int _json_err_t;

struct json_string {
	unsigned int	size; /* data size in symbols */
	char		*data;
};

struct json_array {
	unsigned int	size; /* number of elements  */
	_json_value_t	**pp_values;
};

struct json_object {
	unsigned int	size; /* number of pairs */
	_json_pair_t	**pp_pairs;
};

struct json_value {
	unsigned char	jvt; /* JSON value type */
	union {
		_json_string_t	string;
		_json_array_t	array;
		_json_object_t	object;
		_json_number_t	number;
	};
};

struct json_pair {
	_json_string_t	name;
	_json_value_t	value;
};

typedef struct {
	_ht_context_t	*p_htc; /* hypertext context */
	_json_object_t	*p_root; /* root object */
	unsigned long	err_pos; /* error position */
}_json_context_t;

#ifdef __cplusplus
extern "C" {
#endif
/* Allocate memory for new JSON context */
_json_context_t *json_create_context(_mem_alloc_t *, _mem_free_t *);
/* Pasrse JSON content */
_json_err_t json_parse(_json_context_t *p_jxc, /* JSON context */
			unsigned char *p_content,
			unsigned long content_size);
_json_value_t *json_select(_json_context_t *p_jxc,
			const char *jpath,
			_json_value_t *p_start_point, /* Can be NULL */
			unsigned int index);
/*...*/
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif