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

/* $Id: php_funcmap.h 274371 2009-01-23 14:02:12Z tony2001 $ */

#ifndef PHP_FUNCMAP_H
#define PHP_FUNCMAP_H

#define PHP_FUNCMAP_VERSION "0.1.0-dev"

extern zend_module_entry funcmap_module_entry;
#define phpext_funcmap_ptr &funcmap_module_entry

#ifdef ZTS
#include "TSRM.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(funcmap)
	zend_bool enabled;
	char *logfile;
	int probability;
	int flush_interval_sec;
ZEND_END_MODULE_GLOBALS(funcmap)

#ifdef ZTS
#define FUNCMAP_G(v) TSRMG(funcmap_globals_id, zend_funcmap_globals *, v)
#else
#define FUNCMAP_G(v) (funcmap_globals.v)
#endif

#endif	/* PHP_FUNCMAP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
