/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2009 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Antony Dovgal <tony@daylessday.org>                          |
  +----------------------------------------------------------------------+
*/

/* $Id: funcmap.c 274405 2009-01-23 17:53:42Z tony2001 $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pthread.h"

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_funcmap.h"

#include "zend.h"
#include "zend_API.h"
#include "zend_constants.h"
#include "zend_compile.h"
#include "zend_extensions.h"
#include "zend_builtin_functions.h"
#include "zend_object_handlers.h"
#include "ext/standard/php_mt_rand.h"
#include "zend_smart_str.h"

ZEND_DECLARE_MODULE_GLOBALS(funcmap)

#ifdef COMPILE_DL_FUNCMAP
ZEND_GET_MODULE(funcmap)
#endif

static HashTable funcmap_hash;
int funcmap_execute_initialized = 0;
int funcmap_enabled_real = 0;
time_t funcmap_next_flush_time = 0;

void (*funcmap_old_execute_ex)(zend_execute_data *execute_data);
void funcmap_execute_ex(zend_execute_data *execute_data);
void (*funcmap_old_execute_internal)(zend_execute_data *current_execute_data, zval *return_value);
void funcmap_execute_internal(zend_execute_data *current_execute_data, zval *return_value);

static char *fm_get_function_name(zend_execute_data *execute_data) /* {{{ */
{
	zend_bool free_classname = 0;
	zend_object *object = NULL;
	zend_function *func = NULL;
	char *class_name = NULL, *current_fname = NULL, *function_name = NULL;
	const char *space;

	if (execute_data && execute_data->func) {
		zend_string *zend_function_name;

		object = (Z_TYPE(execute_data->This) == IS_OBJECT) ? Z_OBJ(execute_data->This) : NULL;

		func = execute_data->func;
		zend_function_name = func->common.function_name;

		if (zend_function_name != NULL) {
			function_name = ZSTR_VAL(zend_function_name);
		} else {
			function_name = NULL;
		}
	} else {
		func = NULL;
		function_name = NULL;
	}

	if (execute_data && function_name) {
		if (object) {
			if (func->common.scope) {
				class_name = ZSTR_VAL(func->common.scope->name);
			} else if (object->handlers->get_class_name == std_object_handlers.get_class_name) {
				class_name = ZSTR_VAL(object->ce->name);
			} else {
				zend_string *str = object->handlers->get_class_name(object);
				class_name = estrdup(ZSTR_VAL(str));
				zend_string_release(str);
				free_classname = 1;
			}
		} else if (func->common.scope) {
			class_name = func->common.scope->name->val;
		} else {
			class_name = "";
		}
	} else {
		class_name = (char *)get_active_class_name(&space);
	}

	char *escaped_fname = NULL;
	if (function_name) {
		int i, fname_len, backslashes = 0;

		if (class_name[0] != '\0') {
			fname_len = spprintf(&current_fname, 1024, "%s::%s\n", class_name, function_name);
		} else {
			fname_len = spprintf(&current_fname, 1024, "%s\n", function_name);
		}

		//count backslashes
		for (i = 0; i < fname_len; i++) {
			if (current_fname[i] == '\\') {
				backslashes++;
			}
		}

		if (backslashes == 0) {
			escaped_fname = current_fname;
		} else {
			char *p;
			//escape the backslashes that LSD doesn't like
			escaped_fname = emalloc(fname_len + backslashes + 1);
			p = escaped_fname;
			for (i = 0; i < fname_len + 1/* copy \0 */; i++) {
				if (current_fname[i] == '\\') {
					*p = '\\';
					p++;
					*p = '\\';
				} else {
					*p = current_fname[i];
				}
				p++;
			}
			efree(current_fname);
		}
	}

	if (free_classname) {
		efree(class_name);
	}

	return escaped_fname;
}
/* }}} */

static void php_funcmap_init_globals(zend_funcmap_globals *funcmap_globals) /* {{{ */
{
	memset(funcmap_globals, 0, sizeof(zend_funcmap_globals));
}
/* }}} */

static char *php_funcmap_get_logfile(void) /* {{{ */
{
	char *logfile = FUNCMAP_G(logfile);
	char *pid_pattern;

	if (!logfile || logfile[0] == '\0') {
		return NULL;
	}

	pid_pattern = strstr(logfile, "%pid%");
	if (pid_pattern != NULL) {
		char *real_logfile = NULL;
		pid_t pid = getpid();

		spprintf(&real_logfile, 1024, "%.*s%ld%s", (int)(pid_pattern - logfile), logfile, (long)pid, pid_pattern + strlen("%pid%"));
		return real_logfile;
	}
	return estrdup(logfile);
}
/* }}} */

static void php_funcmap_write_and_cleanup_map(int in_shutdown) /* {{{ */
{
	char *logfile = php_funcmap_get_logfile();
	
	if (!logfile) {
		zend_hash_clean(&funcmap_hash);
		return;
	}

	FILE *fp = fopen(logfile, "a");
	if (!fp) {
		if (in_shutdown) {
			fprintf(stderr, "failed to open file %s for writing: %s", logfile, strerror(errno));
		} else {
			zend_error(E_CORE_WARNING, "failed to open file %s for writing: %s", logfile, strerror(errno));
		}
		efree(logfile);
		return;
	}
	efree(logfile);

	zend_string *key;
	ZEND_HASH_FOREACH_STR_KEY(&funcmap_hash, key) {
		fwrite(ZSTR_VAL(key), ZSTR_LEN(key), 1, fp);
	} ZEND_HASH_FOREACH_END();

	fclose(fp);
	zend_hash_clean(&funcmap_hash);
}
/* }}} */

static void php_funcmap_atfork_child(void) /* {{{ */
{
	if (funcmap_execute_initialized) {
		//compute the probability for each child process separately
		funcmap_enabled_real = 1;
		if (FUNCMAP_G(probability) < 100) {
			//the RNG was seeded in the parent, we want to re-seed it again
			//in order to get different result in children
			BG(mt_rand_is_seeded) = 0;

			long rand_num = php_mt_rand_common(1, 100);
			if (rand_num <= FUNCMAP_G(probability)) {
				funcmap_enabled_real = 1;
			} else {
				funcmap_enabled_real = 0;
			}
		}

		//cleanup the functions hasmap
		zend_hash_clean(&funcmap_hash);

		//and reset the timer if needed
		if (FUNCMAP_G(flush_interval_sec) > 0) {
			time_t now = time(NULL);
			funcmap_next_flush_time = now + FUNCMAP_G(flush_interval_sec);
		}
	}
}
/* }}} */

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("funcmap.enabled",         "0", PHP_INI_SYSTEM, OnUpdateBool, enabled, zend_funcmap_globals, funcmap_globals)
    STD_PHP_INI_ENTRY("funcmap.logfile",         "", PHP_INI_ALL, OnUpdateString, logfile, zend_funcmap_globals, funcmap_globals)
    STD_PHP_INI_ENTRY("funcmap.probability",     "100", PHP_INI_SYSTEM, OnUpdateLong, probability, zend_funcmap_globals, funcmap_globals)
    STD_PHP_INI_ENTRY("funcmap.flush_interval_sec", "0", PHP_INI_ALL, OnUpdateLong, flush_interval_sec, zend_funcmap_globals, funcmap_globals)
PHP_INI_END()
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(funcmap)
{
	ZEND_INIT_MODULE_GLOBALS(funcmap, php_funcmap_init_globals, NULL);

	//we don't want child processes to log functions called in the parent
	if (pthread_atfork(NULL, NULL, php_funcmap_atfork_child) != 0) {
		return FAILURE;
	}

	REGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(funcmap)
{
	UNREGISTER_INI_ENTRIES();

	if (funcmap_execute_initialized) {
		zend_execute_ex = funcmap_old_execute_ex;

		if (funcmap_enabled_real) {
			php_funcmap_write_and_cleanup_map(1);
		}
		zend_hash_destroy(&funcmap_hash);
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(funcmap)
{
	if (!FUNCMAP_G(enabled)) {
		return SUCCESS;
	}

	//the extension is enabled, but should we start the logging?
	funcmap_enabled_real = 1;
	if (FUNCMAP_G(probability) < 100) {
		long rand_num = php_mt_rand_common(1, 100);
		if (rand_num < FUNCMAP_G(probability)) {
			funcmap_enabled_real = 1;
		} else {
			funcmap_enabled_real = 0;
		}
	}

	//have to initialize in any case, children might need this
	if (!funcmap_execute_initialized) {
		funcmap_old_execute_ex = zend_execute_ex;
		zend_execute_ex = funcmap_execute_ex;

		zend_hash_init(&funcmap_hash, 1024, NULL, NULL, 1 /* persistent */);
		funcmap_execute_initialized = 1;

		if (FUNCMAP_G(flush_interval_sec) > 0) {
			funcmap_next_flush_time = time(NULL) + FUNCMAP_G(flush_interval_sec);
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(funcmap)
{
	if (!FUNCMAP_G(enabled)) {
		return SUCCESS;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(funcmap)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "funcmap support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

static PHP_FUNCTION(funcmap_enable) /* {{{ */
{
	zend_bool enable;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "b", &enable) == FAILURE) {
		return;
	}

	if (enable) {
		if (!FUNCMAP_G(enabled)) {
			php_error_docref(NULL, E_WARNING, "trying to enable funcmap logging, but the extension is disabled");
			RETURN_FALSE;
		}

		funcmap_enabled_real = 1;
	} else {
		funcmap_enabled_real = 0;
	}

	RETURN_TRUE;
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_funcmap_enable, 0, 0, 1)
	ZEND_ARG_INFO(0, enable)
ZEND_END_ARG_INFO()

/* {{{ funcmap_functions[]
 */
zend_function_entry funcmap_functions[] = {
	PHP_FE(funcmap_enable, arginfo_funcmap_enable)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ funcmap_module_entry
 */
zend_module_entry funcmap_module_entry = {
	STANDARD_MODULE_HEADER,
	"funcmap",
	funcmap_functions,
	PHP_MINIT(funcmap),
	PHP_MSHUTDOWN(funcmap),
	PHP_RINIT(funcmap),
	PHP_RSHUTDOWN(funcmap),
	PHP_MINFO(funcmap),
	PHP_FUNCMAP_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

void funcmap_execute_ex(zend_execute_data *execute_data) /* {{{ */
{
	//funcmap_enabled_real is set per-process
	if (funcmap_enabled_real) {
		char *fname = fm_get_function_name(execute_data);
		if (fname) {
			zend_hash_str_add_empty_element(&funcmap_hash, fname, strlen(fname));
			efree(fname);
		}

		if (FUNCMAP_G(flush_interval_sec) > 0) {
			time_t now = time(NULL);

			if (funcmap_next_flush_time < now) {
				php_funcmap_write_and_cleanup_map(0);
				funcmap_next_flush_time = now + FUNCMAP_G(flush_interval_sec);
			}
		}
	}

	funcmap_old_execute_ex(execute_data);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
