/**
 * The MeCab PHP extension
 *
 * Copyright (c) 2006-2015 Ryusuke SEKIYAMA. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @package     php-mecab
 * @author      Ryusuke SEKIYAMA <rsky0711@gmail.com>
 * @copyright   2006-2015 Ryusuke SEKIYAMA
 * @license     http://www.opensource.org/licenses/mit-license.php  MIT License
 * @version     $Id$
 */

#include "php_mecab.h"
#include "php_mecab_compat7.h"
#if PHP_VERSION_ID >= 80000
#include "mecab_arginfo.h"
#else
#include "mecab_legacy_arginfo.h"
#endif
#include <ext/spl/spl_exceptions.h>

#define PATHBUFSIZE (MAXPATHLEN + 3)
#define ZEND_HAS_DEFAULT_OBJECT_HANDLERS (ZEND_EXTENSION_API_NO >= 420230831)

/* {{{ globals */

static ZEND_DECLARE_MODULE_GLOBALS(mecab)

/* }}} */

/* {{{ class entries */

static zend_class_entry *ce_MeCab_Tagger = NULL;
static zend_class_entry *ce_MeCab_Node = NULL;
static zend_class_entry *ce_MeCab_NodeIterator = NULL;
static zend_class_entry *ce_MeCab_Path = NULL;

static zend_object_handlers php_mecab_object_handlers;
static zend_object_handlers php_mecab_node_object_handlers;
static zend_object_handlers php_mecab_path_object_handlers;

/* }}} */

/* {{{ module function prototypes */

static PHP_MINIT_FUNCTION(mecab);
static PHP_MSHUTDOWN_FUNCTION(mecab);
static PHP_MINFO_FUNCTION(mecab);
static PHP_GINIT_FUNCTION(mecab);

/* }}} */

/* {{{ internal function prototypes */

/* allocate for mecab */
static php_mecab *
php_mecab_ctor();

/* free the mecab */
static void
php_mecab_dtor(php_mecab *mecab);

/* set string to the mecab */
static void
php_mecab_set_string(php_mecab *mecab, zend_string *str);

/* allocate for mecab_node */
static php_mecab_node *
php_mecab_node_ctor();

/* free the mecab_node */
static void
php_mecab_node_dtor(php_mecab_node *node);

/* set mecab to the mecab_node */
static void
php_mecab_node_set_tagger(php_mecab_node *node, php_mecab *mecab);

/* allocate for mecab_path */
static php_mecab_path *
php_mecab_path_ctor();

/* free the mecab_path */
static void
php_mecab_path_dtor(php_mecab_path *path);

/* set mecab_node to the mecab_path */
static void
php_mecab_path_set_tagger(php_mecab_path *path, php_mecab *mecab);

/* get sibling node from mecab_node */
static zval *
php_mecab_node_get_sibling(zval *zv, zval *object, php_mecab_node *xnode, php_mecab_node_rel rel);

/* get related path from mecab_node */
static zval *
php_mecab_node_get_path(zval *zv, zval *object, php_mecab_node *xnode, php_mecab_node_rel rel);

/* get sibling path from mecab_path */
static zval *
php_mecab_path_get_sibling(zval *zv, zval *object, php_mecab_path *xpath, php_mecab_path_rel rel);

/* get related node from mecab_path */
static zval *
php_mecab_path_get_node(zval *zv, zval *object, php_mecab_path *xpath, php_mecab_path_rel rel);

/* wrappers */
static void
php_mecab_node_get_sibling_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_node_rel rel),
php_mecab_node_get_path_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_node_rel rel),
php_mecab_path_get_sibling_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_path_rel rel),
php_mecab_path_get_node_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_path_rel rel);

/* allocate for mecab object */
zend_object *
php_mecab_object_new(zend_class_entry *ce);

/* free the mecab object */
static void
php_mecab_free_object_storage(zend_object *object);

/* fetch the mecab object */
static inline php_mecab_object * php_mecab_object_fetch_object(zend_object *obj) {
	return (php_mecab_object *)((char *)obj - XtOffsetOf(php_mecab_object, std));
}
#define PHP_MECAB_OBJECT_P(zv) php_mecab_object_fetch_object(Z_OBJ_P(zv))

/* allocate for mecab_node object */
zend_object *
php_mecab_node_object_new(zend_class_entry *ce);

/* free the mecab_node object */
static void
php_mecab_node_free_object_storage(zend_object *object);

/* fetch the mecab_node object */
static inline php_mecab_node_object * php_mecab_node_object_fetch_object(zend_object *obj) {
	return (php_mecab_node_object *)((char *)obj - XtOffsetOf(php_mecab_node_object, std));
}
#define PHP_MECAB_NODE_OBJECT_P(zv) php_mecab_node_object_fetch_object(Z_OBJ_P(zv))

/* allocate for mecab_path object */
zend_object *
php_mecab_path_object_new(zend_class_entry *ce);

/* free the mecab_path object */
static void
php_mecab_path_free_object_storage(zend_object *object);

/* fetch the mecab_path object */
static inline php_mecab_path_object * php_mecab_path_object_fetch_object(zend_object *obj) {
	return (php_mecab_path_object *)((char *)obj - XtOffsetOf(php_mecab_path_object, std));
}
#define PHP_MECAB_PATH_OBJECT_P(zv) php_mecab_path_object_fetch_object(Z_OBJ_P(zv))

/* get the class entry */
static zend_class_entry *
php_mecab_get_class_entry(const char *lcname);

/* }}} */

/* check file/dicectory accessibility */
static zend_bool
php_mecab_check_path(const char *path, size_t length, char *real_path);

/* }}} */

/* {{{ cross-extension dependencies */

static zend_module_dep mecab_deps[] = {
	ZEND_MOD_REQUIRED("spl")
	ZEND_MOD_END
};

/* }}} */

/* {{{ mecab_module_entry */
zend_module_entry mecab_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	mecab_deps,
	"mecab",
	ext_functions,
	PHP_MINIT(mecab),
	PHP_MSHUTDOWN(mecab),
	NULL,
	NULL,
	PHP_MINFO(mecab),
	PHP_MECAB_MODULE_VERSION,
	PHP_MODULE_GLOBALS(mecab),
	PHP_GINIT(mecab),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_MECAB
ZEND_GET_MODULE(mecab)
#endif

/* {{{ ini entries */

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("mecab.default_rcfile", "", PHP_INI_ALL,
		OnUpdateString, default_rcfile, zend_mecab_globals, mecab_globals)
	STD_PHP_INI_ENTRY("mecab.default_dicdir", "", PHP_INI_ALL,
		OnUpdateString, default_dicdir, zend_mecab_globals, mecab_globals)
	STD_PHP_INI_ENTRY("mecab.default_userdic", "", PHP_INI_ALL,
		OnUpdateString, default_userdic, zend_mecab_globals, mecab_globals)
PHP_INI_END()

/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
static PHP_MINIT_FUNCTION(mecab)
{
	REGISTER_INI_ENTRIES();

	register_mecab_symbols(module_number);

	{
		ce_MeCab_Tagger = register_class_MeCab_Tagger();
		ce_MeCab_Tagger->create_object = php_mecab_object_new;
#if ZEND_HAS_DEFAULT_OBJECT_HANDLERS
		ce_MeCab_Tagger->default_object_handlers = &php_mecab_object_handlers;
#endif

		memcpy(&php_mecab_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
		php_mecab_object_handlers.clone_obj = NULL;
		php_mecab_object_handlers.free_obj = php_mecab_free_object_storage;
		php_mecab_object_handlers.offset = XtOffsetOf(php_mecab_object, std);
	}
	{
		ce_MeCab_NodeIterator = register_class_MeCab_NodeIterator(zend_ce_iterator);
		ce_MeCab_NodeIterator->create_object = php_mecab_node_object_new;

		ce_MeCab_Node = register_class_MeCab_Node(zend_ce_aggregate);
		ce_MeCab_Node->create_object = php_mecab_node_object_new;
#if ZEND_HAS_DEFAULT_OBJECT_HANDLERS
		ce_MeCab_Node->default_object_handlers = &php_mecab_node_object_handlers;
#endif

		memcpy(&php_mecab_node_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
		php_mecab_node_object_handlers.clone_obj = NULL;
		php_mecab_node_object_handlers.free_obj = php_mecab_node_free_object_storage;
		php_mecab_node_object_handlers.offset = XtOffsetOf(php_mecab_node_object, std);

		zend_declare_class_constant_long(ce_MeCab_Node, "NOR", 3, MECAB_NOR_NODE);
		zend_declare_class_constant_long(ce_MeCab_Node, "UNK", 3, MECAB_UNK_NODE);
		zend_declare_class_constant_long(ce_MeCab_Node, "BOS", 3, MECAB_BOS_NODE);
		zend_declare_class_constant_long(ce_MeCab_Node, "EOS", 3, MECAB_EOS_NODE);

		zend_declare_class_constant_long(ce_MeCab_Node, "TRAVERSE_NEXT", 13, (zend_long)TRAVERSE_NEXT);
		zend_declare_class_constant_long(ce_MeCab_Node, "TRAVERSE_ENEXT", 14, (zend_long)TRAVERSE_ENEXT);
		zend_declare_class_constant_long(ce_MeCab_Node, "TRAVERSE_BNEXT", 14, (zend_long)TRAVERSE_BNEXT);
	}
	{
		ce_MeCab_Path = register_class_MeCab_Path();
		ce_MeCab_Path->create_object = php_mecab_path_object_new;
#if ZEND_HAS_DEFAULT_OBJECT_HANDLERS
		ce_MeCab_Path->default_object_handlers = &php_mecab_path_object_handlers;
#endif

		memcpy(&php_mecab_path_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
		php_mecab_path_object_handlers.clone_obj = NULL;
		php_mecab_path_object_handlers.free_obj = php_mecab_path_free_object_storage;
		php_mecab_path_object_handlers.offset = XtOffsetOf(php_mecab_path_object, std);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
static PHP_MSHUTDOWN_FUNCTION(mecab)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
static PHP_MINFO_FUNCTION(mecab)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "MeCab Support", "enabled");
	php_info_print_table_row(2, "Module Version", PHP_MECAB_MODULE_VERSION);
	php_info_print_table_end();

	php_info_print_table_start();
	php_info_print_table_header(3, "Version Info", "Compiled", "Linked");
	php_info_print_table_row(3, "MeCab Library", PHP_MECAB_VERSION_STRING, mecab_version());
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/* {{{ PHP_GINIT_FUNCTION */
static PHP_GINIT_FUNCTION(mecab)
{
	mecab_globals->default_rcfile = NULL;
	mecab_globals->default_dicdir = NULL;
	mecab_globals->default_userdic = NULL;
}
/* }}} */

/* {{{ internal function implementation for mecab_t */

/* {{{ php_mecab_ctor()
 * allocate for mecab
 */
static php_mecab *
php_mecab_ctor()
{
	php_mecab *mecab = NULL;

	mecab = (php_mecab *)ecalloc(1, sizeof(php_mecab));
	if (mecab == NULL) {
		return NULL;
	}

	mecab->ptr = NULL;
	mecab->str = NULL;
	mecab->ref = 1;

	return mecab;
}
/* }}} */

/* {{{ php_mecab_dtor()
 * free the mecab
 */
static void
php_mecab_dtor(php_mecab *mecab)
{
	mecab->ref--;
	if (mecab->ref == 0) {
		if (mecab->str != NULL) {
			zend_string_release(mecab->str);
		}
		mecab_destroy(mecab->ptr);
		efree(mecab);
	}
}
/* }}} */

/* {{{ php_mecab_set_string()
 * set string to the mecab
 */
static void
php_mecab_set_string(php_mecab *mecab, zend_string *str)
{
	if (mecab->str != NULL) {
		zend_string_release(mecab->str);
	}
	if (str == NULL) {
		mecab->str = NULL;
	} else {
		mecab->str = zend_string_copy(str);
	}
}
/* }}} */

/* {{{ php_mecab_object_new()
 * allocate for mecab object
 */
zend_object *
php_mecab_object_new(zend_class_entry *ce)
{
	php_mecab_object *intern;

	intern = (php_mecab_object *)ecalloc(1, sizeof(php_mecab_object));
	intern->ptr = php_mecab_ctor();

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);

#if !ZEND_HAS_DEFAULT_OBJECT_HANDLERS
	intern->std.handlers = &php_mecab_object_handlers;
#endif

	return &intern->std;
}
/* }}} */

/* {{{ php_mecab_free_object_storage()
 * free the mecab object
 */
static void
php_mecab_free_object_storage(zend_object *object)
{
	php_mecab_object *intern = php_mecab_object_fetch_object(object);
	php_mecab_dtor(intern->ptr);
	zend_object_std_dtor(&intern->std);
}
/* }}} */

/* }}} mecab_t */

/* {{{ internal function implementation for mecab_node_t */

/* {{{ php_mecab_node_ctor()
 * allocate for mecab_node
 */
static php_mecab_node *
php_mecab_node_ctor()
{
	php_mecab_node *node = NULL;

	node = (php_mecab_node *)ecalloc(1, sizeof(php_mecab_node));
	if (node == NULL) {
		return NULL;
	}

	node->tagger = NULL;
	node->ptr = NULL;

	return node;
}
/* }}} */

/* {{{ php_mecab_node_dtor()
 * free the mecab_node
 */
static void
php_mecab_node_dtor(php_mecab_node *node)
{
	if (node->tagger != NULL) {
		php_mecab_dtor(node->tagger);
	}
	efree(node);
}
/* }}} */

/* {{{ php_mecab_node_set_tagger()
 * set mecab to the mecab_node
 */
static void
php_mecab_node_set_tagger(php_mecab_node *node, php_mecab *mecab)
{
	if (node->tagger != NULL) {
		php_mecab_dtor(node->tagger);
	}
	if (mecab == NULL) {
		node->tagger = NULL;
	} else {
		node->tagger = mecab;
		node->tagger->ref++;
	}
}
/* }}} */

/* {{{ php_mecab_node_object_new()
 * allocate for mecab_node object
 */
zend_object *
php_mecab_node_object_new(zend_class_entry *ce)
{
	php_mecab_node_object *intern;

	intern = (php_mecab_node_object *)ecalloc(1, sizeof(php_mecab_node_object));
	intern->ptr = php_mecab_node_ctor();
	intern->mode = TRAVERSE_NEXT;

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
#if !ZEND_HAS_DEFAULT_OBJECT_HANDLERS
	intern->std.handlers = &php_mecab_node_object_handlers;
#endif

	return &intern->std;
}
/* }}} */

/* {{{ php_mecab_node_free_object_storage()
 * free the mecab_node object
 */
static void
php_mecab_node_free_object_storage(zend_object *object)
{
	php_mecab_node_object *intern = php_mecab_node_object_fetch_object(object);
	php_mecab_node_dtor(intern->ptr);
	zend_object_std_dtor(&intern->std);
}
/* }}} */

/* }}} mecab_node_t */

/* {{{ php_mecab_path_ctor()
 * allocate for mecab_path
 */
static php_mecab_path *
php_mecab_path_ctor()
{
	php_mecab_path *path = NULL;

	path = (php_mecab_path *)ecalloc(1, sizeof(php_mecab_path));
	if (path == NULL) {
		return NULL;
	}

	path->tagger = NULL;
	path->ptr = NULL;

	return path;
}
/* }}} */

/* {{{ php_mecab_path_dtor()
 * free the mecab_path
 */
static void
php_mecab_path_dtor(php_mecab_path *path)
{
	if (path->tagger != NULL) {
		php_mecab_dtor(path->tagger);
	}
	efree(path);
}
/* }}} */

/* {{{ php_mecab_path_set_tagger()
 * set mecab_node to the mecab_path
 */
static void
php_mecab_path_set_tagger(php_mecab_path *path, php_mecab *mecab)
{
	if (path->tagger != NULL) {
		php_mecab_dtor(path->tagger);
	}
	if (mecab == NULL) {
		path->tagger = NULL;
	} else {
		path->tagger = mecab;
		path->tagger->ref++;
	}
}
/* }}} */

/* {{{ php_mecab_path_object_new()
 * allocate for mecab_path object
 */
zend_object *
php_mecab_path_object_new(zend_class_entry *ce)
{
	php_mecab_path_object *intern;

	intern = (php_mecab_path_object *)ecalloc(1, sizeof(php_mecab_path_object));
	intern->ptr = php_mecab_path_ctor();

	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
#if !ZEND_HAS_DEFAULT_OBJECT_HANDLERS
	intern->std.handlers = &php_mecab_path_object_handlers;
#endif

	return &intern->std;
}
/* }}} */

/* {{{ php_mecab_path_free_object_storage()
 * free the mecab_path object
 */
static void
php_mecab_path_free_object_storage(zend_object *object)
{
	php_mecab_path_object *intern = php_mecab_path_object_fetch_object(object);
	php_mecab_path_dtor(intern->ptr);
	zend_object_std_dtor(&intern->std);
}
/* }}} */

/* }}} mecab_path_t */

/* {{{ php_mecab_node_get_sibling()
 * get sibling node from mecab_node
 */
static zval *
php_mecab_node_get_sibling(zval *zv, zval *object, php_mecab_node *xnode, php_mecab_node_rel rel)
{
	const mecab_node_t *node = xnode->ptr;
	php_mecab_node *xsbl = NULL;
	const mecab_node_t *sbl = NULL;
	zval *retval = NULL;

	if (zv == NULL) {
		retval = (zval *)emalloc(sizeof(zval));
	} else {
		zval_dtor(zv);
		retval = zv;
	}

	/* scan */
	if (rel == NODE_PREV) {
		sbl = node->prev;
	} else if (rel == NODE_NEXT) {
		sbl = node->next;
	} else if (rel == NODE_ENEXT) {
		sbl = node->enext;
	} else if (rel == NODE_BNEXT) {
		sbl = node->bnext;
	} else {
		ZVAL_FALSE(retval);
		return retval;
	}

	if (sbl == NULL) {
		ZVAL_NULL(retval);
		return retval;
	}

	/* set return value */
	{
		php_mecab_node_object *newobj;
		object_init_ex(retval, ce_MeCab_Node);
		newobj = PHP_MECAB_NODE_OBJECT_P(retval);
		xsbl = newobj->ptr;
		xsbl->ptr = sbl;
	}
	php_mecab_node_set_tagger(xsbl, xnode->tagger);

	return retval;
}
/* }}} */

/* {{{ php_mecab_node_get_path()
 * get related path from mecab_node
 */
static zval *
php_mecab_node_get_path(zval *zv, zval *object, php_mecab_node *xnode, php_mecab_node_rel rel)
{
	const mecab_node_t *node = xnode->ptr;
	php_mecab_path *xpath = NULL;
	const mecab_path_t *path = NULL;
	zval *retval = NULL;

	if (zv == NULL) {
		retval = (zval *)emalloc(sizeof(zval));
	} else {
		zval_dtor(zv);
		retval = zv;
	}

	/* scan */
	if (rel == NODE_RPATH) {
		path = node->rpath;
	} else if (rel == NODE_LPATH) {
		path = node->lpath;
	} else {
		ZVAL_FALSE(retval);
		return retval;
	}

	if (path == NULL) {
		ZVAL_NULL(retval);
		return retval;
	}

	/* set return value */
	{
		php_mecab_path_object *newobj;
		object_init_ex(retval, ce_MeCab_Path);
		newobj = PHP_MECAB_PATH_OBJECT_P(retval);
		xpath = newobj->ptr;
		xpath->ptr = path;
	}
	php_mecab_path_set_tagger(xpath, xnode->tagger);

	return retval;
}
/* }}} */

/* {{{ php_mecab_path_get_sibling()
 * get sibling path from mecab_path
 */
static zval *
php_mecab_path_get_sibling(zval *zv, zval *object, php_mecab_path *xpath, php_mecab_path_rel rel)
{
	const mecab_path_t *path = xpath->ptr;
	php_mecab_path *xsbl = NULL;
	const mecab_path_t *sbl = NULL;
	zval *retval = NULL;

	if (zv == NULL) {
		retval = (zval *)emalloc(sizeof(zval));
	} else {
		zval_dtor(zv);
		retval = zv;
	}

	/* scan */
	if (rel == PATH_RNEXT) {
		sbl = path->rnext;
	} else if (rel == PATH_LNEXT) {
		sbl = path->lnext;
	} else {
		ZVAL_FALSE(retval);
		return retval;
	}

	if (sbl == NULL) {
		ZVAL_NULL(retval);
		return retval;
	}

	/* set return value */
	{
		php_mecab_path_object *newobj;
		object_init_ex(retval, ce_MeCab_Path);
		newobj = PHP_MECAB_PATH_OBJECT_P(retval);
		xsbl = newobj->ptr;
		xsbl->ptr = sbl;
	}
	php_mecab_path_set_tagger(xsbl, xpath->tagger);

	return retval;
}
/* }}} */

/* {{{ php_mecab_path_get_node()
 * get related node from mecab_path
 */
static zval *
php_mecab_path_get_node(zval *zv, zval *object, php_mecab_path *xpath, php_mecab_path_rel rel)
{
	const mecab_path_t *path = xpath->ptr;
	php_mecab_node *xnode = NULL;
	const mecab_node_t *node = NULL;
	zval *retval = NULL;

	if (zv == NULL) {
		retval = (zval *)emalloc(sizeof(zval));
	} else {
		zval_dtor(zv);
		retval = zv;
	}

	/* scan */
	if (rel == PATH_RNODE) {
		node = path->rnode;
	} else if (rel == PATH_LNODE) {
		node = path->lnode;
	} else {
		ZVAL_FALSE(retval);
		return retval;
	}

	if (node == NULL) {
		ZVAL_NULL(retval);
		return retval;
	}

	/* set return value */
	{
		php_mecab_node_object *newobj;
		object_init_ex(retval, ce_MeCab_Node);
		newobj = PHP_MECAB_NODE_OBJECT_P(retval);
		xnode = newobj->ptr;
		xnode->ptr = node;
	}
	php_mecab_node_set_tagger(xnode, xpath->tagger);

	return retval;
}
/* }}} */

/* {{{ php_mecab_node_get_sibling_wrapper()
 * wraps php_mecab_node_get_sibling()
 */
static void
php_mecab_node_get_sibling_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_node_rel rel)
{
	/* declaration of the resources */
	php_mecab_node *xnode = NULL;

	/* parse the arguments */
	PHP_MECAB_NODE_INTERNAL_FROM_PARAMETER();

	php_mecab_node_get_sibling(return_value, ZEND_THIS, xnode, rel);
}
/* }}} */

/* {{{ php_mecab_node_get_path_wrapper()
 * wraps php_mecab_node_get_path()
 */
static void
php_mecab_node_get_path_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_node_rel rel)
{
	/* declaration of the resources */
	php_mecab_node *xnode = NULL;

	/* parse the arguments */
	PHP_MECAB_NODE_INTERNAL_FROM_PARAMETER();

	php_mecab_node_get_path(return_value, ZEND_THIS, xnode, rel);
}
/* }}} */

/* {{{ php_mecab_path_get_sibling_wrapper()
 * wraps php_mecab_path_get_sibling()
 */
static void
php_mecab_path_get_sibling_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_path_rel rel)
{
	/* declaration of the resources */
	php_mecab_path *xpath = NULL;

	/* parse the arguments */
	PHP_MECAB_PATH_INTERNAL_FROM_PARAMETER();

	php_mecab_path_get_sibling(return_value, ZEND_THIS, xpath, rel);
}
/* }}} */

/* {{{ php_mecab_path_get_node_wrapper()
 * wraps php_mecab_path_get_path()
 */
static void
php_mecab_path_get_node_wrapper(INTERNAL_FUNCTION_PARAMETERS, php_mecab_path_rel rel)
{
	/* declaration of the resources */
	php_mecab_path *xpath = NULL;

	/* parse the arguments */
	PHP_MECAB_PATH_INTERNAL_FROM_PARAMETER();

	php_mecab_path_get_node(return_value, ZEND_THIS, xpath, rel);
}
/* }}} */

/* {{{ php_mecab_get_class_entry()
 * get the class entry
 */
static zend_class_entry *
php_mecab_get_class_entry(const char *lcname)
{
	zval *entry = zend_hash_str_find(CG(class_table), lcname, strlen(lcname));
	if (entry && Z_TYPE_P(entry) == IS_PTR) {
		return Z_PTR_P(entry);
	} else {
		return NULL;
	}
}
/* }}} */

/* {{{ php_mecab_node_list_method()
 * check file/dicectory accessibility
 */
static zend_bool
php_mecab_check_path(const char *path, size_t length, char *real_path)
{
	char *full_path;

	if (strlen(path) != length)
	{
		return 0;
	}

	if (strchr(path, ',') != NULL) /* multiple paths */
	{
		/* don't expand paths or changes will be huge for such a small thing */
		if (real_path != NULL) {
			if (length >= PATHBUFSIZE) { /* XXX: assuming buffer size */
				return 0;
			}
			strcpy(real_path, path);
		}
		return 1;
	}

	if ((full_path = expand_filepath(path, real_path)) == NULL)
	{
		return 0;
	}

	if (VCWD_ACCESS(full_path, F_OK) != 0 ||
		VCWD_ACCESS(full_path, R_OK) != 0 ||
		php_check_open_basedir(full_path))
	{
		if (real_path == NULL) {
			efree(full_path);
		}
		return 0;
	}

#if !defined(PHP_VERSION_ID) || PHP_VERSION_ID < 50400
	if (PG(safe_mode) && !php_checkuid(full_path, NULL, CHECKUID_CHECK_FILE_AND_DIR)) {
		if (real_path == NULL) {
			efree(full_path);
		}
		return 0;
	}
#endif

	if (real_path == NULL) {
		efree(full_path);
	}
	return 1;
}
/* }}} */

/* {{{ macro for checking constructor options */

/* check if default parameter is set */
#define PHP_MECAB_CHECK_DEFAULT(name) \
	(MECAB_G(default_##name) != NULL && MECAB_G(default_##name)[0] != '\0')

#define PHP_MECAB_GETOPT_FAILURE -1
#define PHP_MECAB_GETOPT_SUCCESS 0
#define PHP_MECAB_GETOPT_FLAG_EXPECTED    (1 << 0)
#define PHP_MECAB_GETOPT_RCFILE_EXPECTED  (1 << 2)
#define PHP_MECAB_GETOPT_DICDIR_EXPECTED  (1 << 3)
#define PHP_MECAB_GETOPT_USERDIC_EXPECTED (1 << 4)
#define PHP_MECAB_GETOPT_PATH_EXPECTED \
	(PHP_MECAB_GETOPT_RCFILE_EXPECTED | PHP_MECAB_GETOPT_DICDIR_EXPECTED | PHP_MECAB_GETOPT_USERDIC_EXPECTED)

/* check for option */
static int
php_mecab_check_option(zend_string *zopt)
{
	const char *opt = ZSTR_VAL(zopt);
	size_t len = ZSTR_LEN(zopt);

	/* not an opt */
	if (*opt != '-') {
		return PHP_MECAB_GETOPT_FAILURE;
	}

	/* resource file */
	if (!zend_binary_strcmp(opt, len, ZEND_STRL("-r")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--rcfile"))) {
		return PHP_MECAB_GETOPT_RCFILE_EXPECTED;
	}

	/* system dicdir */
	if (!zend_binary_strcmp(opt, len, ZEND_STRL("-d")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--dicdir"))) {
		return PHP_MECAB_GETOPT_DICDIR_EXPECTED;
	}

	/* user dictionary */
	if (!zend_binary_strcmp(opt, len, ZEND_STRL("-u")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--userdic"))) {
		return PHP_MECAB_GETOPT_USERDIC_EXPECTED;
	}

	/* options whose parameter is not filename */
	if (!zend_binary_strcmp(opt, len, ZEND_STRL("-l")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--lattice-level")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-O")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--output-format-type")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-F")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--node-format")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-U")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--unk-format")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-B")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--bos-format")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-E")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--eos-format")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-x")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--unk-feature")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-b")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--input-buffer-size")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-N")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--nbest")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-t")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--theta")))
	{
		return PHP_MECAB_GETOPT_SUCCESS;
	}

	/* options which does not have parameter */
	if (!zend_binary_strcmp(opt, len, ZEND_STRL("-a")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--all-morphs")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-p")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--partial")) ||
		!zend_binary_strcmp(opt, len, ZEND_STRL("-C")) || !zend_binary_strcmp(opt, len, ZEND_STRL("--allocate-sentence")))
	{
		return (PHP_MECAB_GETOPT_SUCCESS | PHP_MECAB_GETOPT_FLAG_EXPECTED);
	}

	/* invalid options */
	return PHP_MECAB_GETOPT_FAILURE;
}

/* check for open_basedir and safe_mode */
#define PHP_MECAB_CHECK_FILE(path, length) \
{ \
	if (!php_mecab_check_path((path), (length), resolved_path)) { \
		efree(argv); \
		php_error_docref(NULL, E_WARNING, "'%s' does not exist or is not readable", (path)); \
		RETURN_FALSE; \
	} \
	flag_expected = 1; \
	path_expected = 0; \
}

/* }}} */

/* {{{ Functions */

/* {{{ proto string MeCab\version(void) */
/**
 * Get the version number of MeCab.
 *
 * @return	string	The version of linked MeCab library.
 */
PHP_FUNCTION(MeCab_version)
{
	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	RETURN_STRING((char *)mecab_version());
}
/* }}} mecab_version */

/* {{{ proto array MeCab\split(string str[, string dicdir[, string userdic]]) */
/**
 * Split string into an array of morphemes.
 *
 * @param	string	$str	The target string.
 * @param	string	$dicdir	The path for system dictionary.
 * @param	string	$userdic	The path for user dictionary.
 * @return	array
 */
PHP_FUNCTION(MeCab_split)
{
	/* variables from argument */
	zend_string *str = NULL;
	zend_string *zdicdir = NULL;
	const char *dicdir = NULL;
	size_t dicdir_len = 0;
	zend_string *zuserdic = NULL;
	const char *userdic = NULL;
	size_t userdic_len = 0;

	/* local variables */
	mecab_t *mecab = NULL;
	const mecab_node_t *node = NULL;
	int argc = 2;
	char *argv[5] = { "mecab", "-Owakati", NULL, NULL, NULL };
	char pathbuf[2][PATHBUFSIZE] = {{'\0'}};
	char *dicdir_buf = &(pathbuf[0][0]);
	char *userdic_buf = &(pathbuf[1][0]);

	/* parse arguments */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|S!S!", &str, &zdicdir, &zuserdic) == FAILURE) {
		return;
	}

	/* apply default options */
	if (zdicdir != NULL && ZSTR_LEN(zdicdir) > 0) {
		dicdir = ZSTR_VAL(zdicdir);
		dicdir_len = ZSTR_LEN(zdicdir);
	} else if (PHP_MECAB_CHECK_DEFAULT(dicdir)) {
		dicdir = MECAB_G(default_dicdir);
		dicdir_len = strlen(MECAB_G(default_dicdir));
	}
	if (zuserdic != NULL && ZSTR_LEN(zuserdic) > 0) {
		userdic = ZSTR_VAL(zuserdic);
		userdic_len = ZSTR_LEN(zuserdic);
	} else if (PHP_MECAB_CHECK_DEFAULT(userdic)) {
		userdic = MECAB_G(default_userdic);
		userdic_len = strlen(MECAB_G(default_userdic));
	}

	/* check for dictionary */
	if (dicdir != NULL && dicdir_len > 0) {
		char *dicdir_ptr = dicdir_buf;
		*dicdir_ptr++ = '-';
		*dicdir_ptr++ = 'd';
		if (!php_mecab_check_path(dicdir, dicdir_len, dicdir_ptr)) {
			php_error_docref(NULL, E_WARNING,
					"'%s' does not exist or is not readable", dicdir);
			RETURN_FALSE;
		}
		argv[argc++] = dicdir_buf;
	}
	if (userdic != NULL && userdic_len > 0) {
		char *userdic_ptr = userdic_buf;
		*userdic_ptr++ = '-';
		*userdic_ptr++ = 'u';
		if (!php_mecab_check_path(userdic, userdic_len, userdic_ptr)) {
			php_error_docref(NULL, E_WARNING,
					"'%s' does not exist or is not readable", userdic);
			RETURN_FALSE;
		}
		argv[argc++] = userdic_buf;
	}

	/* create mecab object */
	mecab = mecab_new(argc, argv);

	/* on error */
	if (mecab == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(NULL));
		RETURN_FALSE;
	}

	/* parse the string */
	node = mecab_sparse_tonode(mecab, ZSTR_VAL(str));
	if (node == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(mecab));
		mecab_destroy(mecab);
		RETURN_FALSE;
	}

	/* initialize */
	array_init(return_value);

	/* put surfaces of each node to return value */
	while (node != NULL) {
		if (node->length > 0) {
			add_next_index_stringl(return_value, (char *)node->surface, (int)node->length);
		}
		node = node->next;
	}

	/* free mecab object */
	mecab_destroy(mecab);
}
/* }}} mecab_split */

/* {{{ proto resource mecab mecab_new([array options]) */
/**
 * resource mecab mecab_new([array options])
 * object MeCab_Tagger MeCab_Tagger::__construct([array options])
 *
 * Create new tagger resource of MeCab.
 *
 * @param	array	$options	The analysis/output options. (optional)
 *								The values are same to command line options.
 *								The detail is found in the web site and/or the manpage of MeCab.
 * @return	resource mecab	A tagger resource of MeCab.
 */
PHP_METHOD(MeCab_Tagger, __construct)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	zval *zoptions = NULL;
	HashTable *options = NULL;

	/* declaration of the local variables */
	size_t min_argc = 5; /* "mecab" + "-r" + "-d" + "-u" + NULL  */
	int argc = 1;
	char **argv = NULL;
	int flag_expected = 1;
	int path_expected = 0;
	char pathbuf[3][PATHBUFSIZE] = {{'\0'}};
	char *rcfile_buf = &(pathbuf[0][0]);
	char *dicdir_buf = &(pathbuf[1][0]);
	char *userdic_buf = &(pathbuf[2][0]);
	char *resolved_path = NULL;

	/* parse arguments */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|a!", &zoptions) == FAILURE) {
		return;
	}

	/* parse options */
	if (zoptions != NULL) {
		int getopt_result = 0;
		zend_string *key;
		zend_ulong num_key;
		zval *entry;

		ALLOC_HASHTABLE(options);

		zend_hash_init(options, zend_hash_num_elements(Z_ARRVAL_P(zoptions)), NULL, ZVAL_PTR_DTOR, 0);
		zend_hash_copy(options, Z_ARRVAL_P(zoptions), (copy_ctor_func_t)zval_add_ref);

		argv = (char **)ecalloc(2 * zend_hash_num_elements(options) + min_argc, sizeof(char *));

		ZEND_HASH_FOREACH_KEY_VAL(options, num_key, key, entry) {
			convert_to_string_ex(entry);
		  if (key) {
				getopt_result = php_mecab_check_option(key);
				if (getopt_result == FAILURE) {
					php_error_docref(NULL, E_WARNING, "Invalid option '%s' given", ZSTR_VAL(key));
					efree(argv);
					RETURN_FALSE;
				} else {
					flag_expected = getopt_result & PHP_MECAB_GETOPT_FLAG_EXPECTED;
					path_expected = getopt_result & PHP_MECAB_GETOPT_PATH_EXPECTED;
					if (getopt_result & PHP_MECAB_GETOPT_RCFILE_EXPECTED) {
						resolved_path = rcfile_buf;
					} else if (getopt_result & PHP_MECAB_GETOPT_DICDIR_EXPECTED) {
						resolved_path = dicdir_buf;
					} else if (getopt_result & PHP_MECAB_GETOPT_USERDIC_EXPECTED) {
						resolved_path = userdic_buf;
					}
				}
				argv[argc++] = ZSTR_VAL(key);
				if (path_expected) {
					PHP_MECAB_CHECK_FILE(Z_STRVAL_P(entry), Z_STRLEN_P(entry));
					argv[argc++] = resolved_path;
				} else {
					argv[argc++] = Z_STRVAL_P(entry);
				}
				flag_expected = 1;
				path_expected = 0;
			} else {
				if (flag_expected) {
					getopt_result = php_mecab_check_option(Z_STR_P(entry));
					if (getopt_result == FAILURE) {
						php_error_docref(NULL, E_WARNING,
								"Invalid option '%s' given", Z_STRVAL_P(entry));
						efree(argv);
						RETURN_FALSE;
					} else {
						flag_expected = getopt_result & PHP_MECAB_GETOPT_FLAG_EXPECTED;
						path_expected = getopt_result & PHP_MECAB_GETOPT_PATH_EXPECTED;
						if (getopt_result & PHP_MECAB_GETOPT_RCFILE_EXPECTED) {
							resolved_path = rcfile_buf;
						} else if (getopt_result & PHP_MECAB_GETOPT_DICDIR_EXPECTED) {
							resolved_path = dicdir_buf;
						} else if (getopt_result & PHP_MECAB_GETOPT_USERDIC_EXPECTED) {
							resolved_path = userdic_buf;
						}
					}
					argv[argc++] = Z_STRVAL_P(entry);
				} else if (path_expected) {
					PHP_MECAB_CHECK_FILE(Z_STRVAL_P(entry), Z_STRLEN_P(entry));
					argv[argc++] = resolved_path;
				} else {
					argv[argc++] = Z_STRVAL_P(entry);
				}
			}
		} ZEND_HASH_FOREACH_END();
	} else {
		argv = (char **)ecalloc(min_argc, sizeof(char *));
	}

	/* apply default options */
	if (rcfile_buf[0] == '\0' && PHP_MECAB_CHECK_DEFAULT(rcfile)) {
		size_t rcfile_len = strlen(MECAB_G(default_rcfile));
		resolved_path = rcfile_buf;
		*resolved_path++ = '-';
		*resolved_path++ = 'r';
		PHP_MECAB_CHECK_FILE(MECAB_G(default_rcfile), rcfile_len);
		argv[argc++] = rcfile_buf;
	}
	if (dicdir_buf[0] == '\0' && PHP_MECAB_CHECK_DEFAULT(dicdir)) {
		size_t dicdir_len = strlen(MECAB_G(default_dicdir));
		resolved_path = dicdir_buf;
		*resolved_path++ = '-';
		*resolved_path++ = 'd';
		PHP_MECAB_CHECK_FILE(MECAB_G(default_dicdir), dicdir_len);
		argv[argc++] = dicdir_buf;
	}
	if (userdic_buf[0] == '\0' && PHP_MECAB_CHECK_DEFAULT(userdic)) {
		size_t userdic_len = strlen(MECAB_G(default_userdic));
		resolved_path = userdic_buf;
		*resolved_path++ = '-';
		*resolved_path++ = 'u';
		PHP_MECAB_CHECK_FILE(MECAB_G(default_userdic), userdic_len);
		argv[argc++] = userdic_buf;
	}

	/* create mecab object */
	argv[0] = "mecab";
	argv[argc] = NULL;
	mecab = mecab_new(argc, argv);

	efree(argv);
	if (options != NULL) {
		zend_hash_destroy(options);
		FREE_HASHTABLE(options);
	}

	/* on error */
	if (mecab == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(NULL));
		RETURN_FALSE;
	}

	{
		const php_mecab_object *intern = PHP_MECAB_OBJECT_P(ZEND_THIS);
		xmecab = intern->ptr;
		if (xmecab->ptr != NULL) {
			mecab_destroy(mecab);
			zend_throw_exception(spl_ce_BadMethodCallException,
					"MeCab already initialized", 0);
			return;
		}
	}
	xmecab->ptr = mecab;
}
/* }}} mecab_new */

/* {{{ proto bool mecab_get_partial(resource mecab mecab) */
/**
 * bool mecab_get_partial(resource mecab mecab)
 * bool MeCab_Tagger::getPartial()
 *
 * Get partial parsing mode.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @return	bool
 */
PHP_METHOD(MeCab_Tagger, getPartial)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* parse the arguments */
	PHP_MECAB_FROM_PARAMETER();

	RETURN_BOOL(mecab_get_partial(mecab));
}
/* }}} */

/* {{{ proto void mecab_set_partial(resource mecab mecab, bool partial) */
/**
 * void mecab_set_partial(resource mecab mecab, bool partial)
 * void MeCab_Tagger::setPartial(bool partial)
 *
 * Set partial parsing mode.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	bool	$partial	The partial parsing mode.
 * @return	void
 */
PHP_METHOD(MeCab_Tagger, setPartial)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	zend_bool partial = 0;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("b", &partial);

	mecab_set_partial(mecab, (int)partial);
}
/* }}} */

/* {{{ proto float mecab_get_theta(resource mecab mecab) */
/**
 * float mecab_get_theta(resource mecab mecab)
 * float MeCab_Tagger::getTheta()
 *
 * Get temparature parameter theta.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @return	float
 */
PHP_METHOD(MeCab_Tagger, getTheta)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* parse the arguments */
	PHP_MECAB_FROM_PARAMETER();

	RETURN_DOUBLE((double)mecab_get_theta(mecab));
}
/* }}} */

/* {{{ proto void mecab_(resource mecab mecab, float theta) */
/**
 * void mecab_set_theta(resource mecab mecab, float theta)
 * void MeCab_Tagger::setTheta(float theta)
 *
 * Set temparature parameter theta.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	float	$theta	The temparature parameter theta.
 * @return	void
 */
PHP_METHOD(MeCab_Tagger, setTheta)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	double theta = 0;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("d", &theta);

	mecab_set_theta(mecab, (float)theta);
}
/* }}} */

/* {{{ proto int mecab_get_lattice_level(resource mecab mecab) */
/**
 * int mecab_get_lattice_level(resource mecab mecab)
 * int MeCab_Tagger::getLatticeLevel()
 *
 * Get lattice information level.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @return	int
 */
PHP_METHOD(MeCab_Tagger, getLatticeLevel)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* parse the arguments */
	PHP_MECAB_FROM_PARAMETER();

	RETURN_LONG((zend_long)mecab_get_lattice_level(mecab));
}
/* }}} */

/* {{{ proto void mecab_set_lattice_level(resource mecab mecab, int level) */
/**
 * void mecab_set_lattice_level(resource mecab mecab, int level)
 * void MeCab_Tagger::setLatticeLevel(int level)
 *
 * Set lattice information level.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	int	$level	The lattice information level.
 * @return	void
 */
PHP_METHOD(MeCab_Tagger, setLatticeLevel)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	zend_long level = 0L;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("l", &level);

	mecab_set_lattice_level(mecab, (int)level);
}
/* }}} */

/* {{{ proto bool mecab_get_all_morphs(resource mecab mecab) */
/**
 * bool mecab_get_all_morphs(resource mecab mecab)
 * bool MeCab_Tagger::getAllMorphs()
 *
 * Get all morphs mode.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @return	bool
 */
PHP_METHOD(MeCab_Tagger, getAllMorphs)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* parse the arguments */
	PHP_MECAB_FROM_PARAMETER();

	RETURN_BOOL(mecab_get_all_morphs(mecab));
}
/* }}} */

/* {{{ proto void mecab_set_all_morphs(resource mecab mecab, int all_morphs) */
/**
 * void mecab_set_all_morphs(resource mecab mecab, int all_morphs)
 * void MeCab_Tagger::setAllMorphs(int all_morphs)
 *
 * Set all morphs mode.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	bool	$all_morphs	The all morphs mode.
 * @return	void
 */
PHP_METHOD(MeCab_Tagger, setAllMorphs)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	zend_bool all_morphs = 0;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("b", &all_morphs);

	mecab_set_all_morphs(mecab, (int)all_morphs);
}
/* }}} */

/* {{{ proto string mecab_sparse_tostr(resource mecab mecab, string str[, int len[, int olen]]) */
/**
 * string mecab_sparse_tostr(resource mecab mecab, string str[, int len[, int olen]])
 * string MeCab_Tagger::parse(string str[, int len[, int olen]])
 * string MeCab_Tagger::parseToString(string str[, int len[, int olen]])
 *
 * Get the parse result as a string.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	string	$str	The parse target.
 * @param	int	$len	The maximum length that can be analyzed. (optional)
 * @param	int	$olen	The limit length of the output buffer. (optional)
 * @return	string	The parse result.
 *					If output buffer has overflowed, returns false.
 */
PHP_METHOD(MeCab_Tagger, parse)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	zend_string *str = NULL;
	long len = 0;
	long olen = 0;

	/* declaration of the local variables */
	size_t ilen = 0;
	char *ostr = NULL;
	zend_bool ostr_free = 0;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("S|ll", &str, &len, &olen);

	/* call mecab_sparse_tostr() */
	php_mecab_set_string(xmecab, str);
	ilen = (size_t)((len > 0) ? MIN(len, (long)ZSTR_LEN(str)) : ZSTR_LEN(str));
	if (olen == 0) {
		ostr = (char *)mecab_sparse_tostr2(mecab, ZSTR_VAL(xmecab->str), ilen);
	} else {
		ostr = (char *)emalloc((size_t)olen + 1);
		ostr = mecab_sparse_tostr3(mecab, ZSTR_VAL(xmecab->str), ilen, ostr, (size_t)olen);
		ostr_free = 1;
	}

	/* set return value */
	if (ostr == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(mecab));
		RETVAL_FALSE;
	} else {
		RETVAL_STRING(ostr);
	}

	/* free */
	if (ostr_free) {
		efree(ostr);
	}
}
/* }}} mecab_sparse_tostr */

/* {{{ proto resource mecab_node mecab_sparse_tonode(resource mecab mecab, string str[, int len]) */
/**
 * resource mecab_node mecab_sparse_tonode(resource mecab mecab, string str[, int len])
 * object MeCab_Node MeCab_Tagger::parseToNode(string str[, int len])
 *
 * Get the parse result as a node.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	string	$str	The parse target.
 * @param	int	$len	The maximum length that can be analyzed. (optional)
 * @return	resource mecab_node	The result node of given string.
 */
PHP_METHOD(MeCab_Tagger, parseToNode)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	zend_string *str = NULL;
	long len = 0;

	/* declaration of the local variables */
	size_t ilen = 0;
	const mecab_node_t *node = NULL;
	php_mecab_node *xnode = NULL;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("S|l", &str, &len);

	/* call mecab_sparse_tonode() */
	php_mecab_set_string(xmecab, str);
	ilen = (size_t)((len > 0) ? MIN(len, (long)ZSTR_LEN(str)) : ZSTR_LEN(str));
	node = mecab_sparse_tonode2(mecab, ZSTR_VAL(xmecab->str), ilen);
	if (node == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(mecab));
		RETURN_FALSE;
	}

	/* set return value */
	{
		php_mecab_node_object *newobj;
		object_init_ex(return_value, ce_MeCab_Node);
		newobj = PHP_MECAB_NODE_OBJECT_P(return_value);
		xnode = newobj->ptr;
	}
	xnode->ptr = node;
	php_mecab_node_set_tagger(xnode, xmecab);
}
/* }}} mecab_sparse_tonode */

/* {{{ proto string mecab_nbest_sparse_tostr(resource mecab mecab, int n, string str[, int len[, int olen]]) */
/**
 * string mecab_nbest_sparse_tostr(resource mecab mecab, int n, string str[, int len[, int olen]])
 * string MeCab_Tagger::parseNBest(int n, string str[, int len[, int olen]])
 *
 * Get the N-Best list as a string.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	int	$n	The number of the result list.
 * @param	string	$str	The parse target.
 * @param	int	$len	The maximum length that can be analyzed. (optional)
 * @param	int	$olen	The maximum length of the output. (optional)
 * @return	string	The N-Best list.
 *					If output buffer has overflowed, returns false.
 */
PHP_METHOD(MeCab_Tagger, parseNBest)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	long n = 0;
	zend_string *str = NULL;
	long len = 0;
	long olen = 0;

	/* declaration of the local variables */
	size_t ilen = 0;
	char *ostr = NULL;
	zend_bool ostr_free = 1;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("lS|ll", &n, &str, &len, &olen);

	/* call mecab_nbest_sparse_tostr() */
	php_mecab_set_string(xmecab, str);
	ilen = (size_t)((len > 0) ? MIN(len, (long)ZSTR_LEN(str)) : ZSTR_LEN(str));
	if (olen == 0) {
		ostr = (char *)mecab_nbest_sparse_tostr2(mecab, n, ZSTR_VAL(xmecab->str), ilen);
	} else {
		ostr = (char *)emalloc(olen + 1);
		ostr = mecab_nbest_sparse_tostr3(mecab, n, ZSTR_VAL(xmecab->str), ilen, ostr, (size_t)olen);
		ostr_free = 1;
	}

	/* set return value */
	if (ostr == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(mecab));
		RETVAL_FALSE;
	} else {
		RETVAL_STRING(ostr);
	}

	/* free */
	if (ostr_free) {
		efree(ostr);
	}
}
/* }}} mecab_nbest_sparse_tostr */

/* {{{ proto bool mecab_nbest_init(resource mecab mecab, string str[, int len]) */
/**
 * bool mecab_nbest_init(resource mecab mecab, string str[, int len])
 * bool MeCab_Tagger::parseNBestInit(string str[, int len])
 *
 * Initialize the N-Best list.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	string	$str	The parse target.
 * @param	int	$len	The maximum length that can be analyzed. (optional)
 * @return	bool	True if succeeded to initilalize, otherwise returns false.
 */
PHP_METHOD(MeCab_Tagger, parseNBestInit)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	zend_string *str = NULL;
	long len = 0;

	/* declaration of the local variables */
	size_t ilen = 0;
	int result = 0;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("S|l", &str, &len);

	/* call mecab_nbest_init() */
	php_mecab_set_string(xmecab, str);
	ilen = (size_t)((len > 0) ? MIN(len, (long)ZSTR_LEN(str)) : ZSTR_LEN(str));
	result = mecab_nbest_init2(mecab, ZSTR_VAL(xmecab->str), ilen);
	if (result == 0) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(mecab));
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} mecab_nbest_init */

/* {{{ proto string mecab_nbest_next_tostr(resource mecab mecab[, int olen]) */
/**
 * string mecab_nbest_next_tostr(resource mecab mecab[, int olen])
 * string MeCab_Tagger::next([int olen]])
 *
 * Get the next result of N-Best as a string.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	int	$olen	The maximum length of the output. (optional)
 * @return	string	The parse result of the next pointer.
 *					If there are no more results, returns false.
 *					Also returns false if output buffer has overflowed.
 */
PHP_METHOD(MeCab_Tagger, next)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the arguments */
	long olen = 0;

	/* declaration of the local variables */
	char *ostr = NULL;
	zend_bool ostr_free = 0;
	const char *what = NULL;

	/* parse the arguments */
	PHP_MECAB_PARSE_PARAMETERS("|l", &olen);

	/* call mecab_nbest_sparse_tostr() */
	if (olen == 0) {
		ostr = (char *)mecab_nbest_next_tostr(mecab);
	} else {
		ostr = (char *)emalloc(olen + 1);
		ostr = mecab_nbest_next_tostr2(mecab, ostr, (size_t)olen);
		ostr_free = 1;
	}

	/* set return value */
	if (ostr == NULL) {
		if ((what = mecab_strerror(mecab)) != NULL &&
			strcmp((char *)what, "no more results"))
		{
			php_error_docref(NULL, E_WARNING, "%s", what);
		}
		RETVAL_FALSE;
	} else {
		RETVAL_STRING(ostr);
	}

	/* free */
	if (ostr_free) {
		efree(ostr);
	}
}
/* }}} mecab_nbest_next_tostr */

/* {{{ proto resource mecab_node mecab_nbest_next_tonode(resource mecab mecab) */
/**
 * resource mecab_node mecab_nbest_next_tonode(resource mecab mecab)
 * object MeCab_Node MeCab_Tagger::nextNode(void)
 *
 * Get the next result of N-Best as a node.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @return	resource mecab_node	The result node of the next pointer.
 *								If there are no more results, returns false.
 */
PHP_METHOD(MeCab_Tagger, nextNode)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the local variables */
	const mecab_node_t *node = NULL;
	php_mecab_node *xnode = NULL;
	const char *what = NULL;

	/* parse the arguments */
	PHP_MECAB_FROM_PARAMETER();

	/* call mecab_nbest_next_tonode() */
	node = mecab_nbest_next_tonode(mecab);
	if (node == NULL) {
		if ((what = mecab_strerror(mecab)) != NULL &&
			strcmp((char *)what, "no more results"))
		{
			php_error_docref(NULL, E_WARNING, "%s", what);
		}
		RETURN_FALSE;
	}

	/* set return value */
	{
		php_mecab_node_object *newobj;
		object_init_ex(return_value, ce_MeCab_Node);
		newobj = PHP_MECAB_NODE_OBJECT_P(return_value);
		xnode = newobj->ptr;
	}
	xnode->ptr = node;
	php_mecab_node_set_tagger(xnode, xmecab);
}
/* }}} mecab_nbest_next_tonode */

/* {{{ proto string mecab_format_node(resource mecab mecab, resource mecab_node node) */
/**
 * string mecab_format_node(resource mecab mecab, resource mecab_node node)
 * string MeCab_Tagger::formatNode(object MeCab_Node node)
 *
 * Format a node to string.
 * The format is specified by "-O" option or --{node|unk|bos|eos}-format=STR.
 * The detail is found in the web site and/or the manpage of MeCab.
 * NOTICE: If the option was "wakati" or "dump", the return string will be empty.
 *
 * @param	resource mecab	$mecab	The tagger resource of MeCab.
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	string	The formatted string.
 * @see	mecab_node_tostring
 */
PHP_METHOD(MeCab_Tagger, formatNode)
{
	/* declaration of the resources */
	zval *node_object = NULL;
	php_mecab *xmecab = NULL;
	php_mecab_node *xnode = NULL;
	mecab_t *mecab = NULL;
	const mecab_node_t *node = NULL;

	/* declaration of the local variables */
	const char *fmt = NULL;

	/* parse the arguments */
	{
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &node_object, ce_MeCab_Node) == FAILURE) {
			return;
		} else {
			const php_mecab_object *intern = PHP_MECAB_OBJECT_P(ZEND_THIS);
			const php_mecab_node_object *intern_node = PHP_MECAB_NODE_OBJECT_P(node_object);
			xmecab = intern->ptr;
			xnode = intern_node->ptr;
		}
	}
	mecab = xmecab->ptr;
	node = xnode->ptr;

	/* call mecab_format_node() */
	fmt = mecab_format_node(mecab, node);
	if (fmt == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(mecab));
		RETURN_FALSE;
	}

	/* set return value */
	RETURN_STRING((char *)fmt);
}
/* }}} mecab_format_node */

/* {{{ proto array mecab_dictionary_info(resource mecab mecab) */
/**
 * array mecab_dictionary_info(resource mecab mecab)
 * array MeCab_Tagger::dictionaryInfo(void)
 *
 * Get the information of using dictionary as an associative array.
 *
 * @return	array	The information of the dictionary.
 */
PHP_METHOD(MeCab_Tagger, dictionaryInfo)
{
	/* declaration of the resources */
	php_mecab *xmecab = NULL;
	mecab_t *mecab = NULL;

	/* declaration of the local variables */
	const mecab_dictionary_info_t *dicinfo = NULL;

	/* parse the arguments */
	PHP_MECAB_FROM_PARAMETER();
	/* get dictionary information */
	dicinfo = mecab_dictionary_info(mecab);
	if (dicinfo == NULL) {
		RETURN_NULL();
	}

	/* initialize */
	array_init(return_value);

	/* get information for each dictionary */
	while (dicinfo != NULL) {
		zval tmp;
		array_init(&tmp);

		add_assoc_string(&tmp, "filename", (char *)dicinfo->filename);
		add_assoc_string(&tmp, "charset",  (char *)dicinfo->charset);
		add_assoc_long(&tmp, "size",    (zend_long)dicinfo->size);
		add_assoc_long(&tmp, "type",    (zend_long)dicinfo->type);
		add_assoc_long(&tmp, "lsize",   (zend_long)dicinfo->lsize);
		add_assoc_long(&tmp, "rsize",   (zend_long)dicinfo->rsize);
		add_assoc_long(&tmp, "version", (zend_long)dicinfo->version);

		add_next_index_zval(return_value, &tmp);

		dicinfo = dicinfo->next;
	}
}
/* }}} mecab_dictionary_info */

/* {{{ proto array mecab_node_toarray(resource mecab_node node[, bool dump_all]) */
/**
 * array mecab_node_toarray(resource mecab_node node[, bool dump_all])
 * array MeCab_Node::toArray([bool dump_all])
 *
 * Get all elements of the node as an associative array.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @param	bool	$dump_all	Whether dump all related nodes and paths or not.
 * @return	array	All elements of the node.
 */
PHP_METHOD(MeCab_Node, toArray)
{
	/* declaration of the resources */
	zval *object = ZEND_THIS;
	php_mecab_node *xnode = NULL;
	const mecab_node_t *node = NULL;

	/* declaration of the arguments */
	zend_bool dump_all = 0;

	/* parse the arguments */
	PHP_MECAB_NODE_PARSE_PARAMETERS("|b", &dump_all);
	node = xnode->ptr;

	/* initialize */
	array_init(return_value);

	/* assign siblings and paths */
	if (dump_all) {
		zval tmp;
		ZVAL_UNDEF(&tmp);
		add_assoc_zval(return_value, "prev",  php_mecab_node_get_sibling(&tmp, object, xnode, NODE_PREV));
		ZVAL_UNDEF(&tmp);
		add_assoc_zval(return_value, "next",  php_mecab_node_get_sibling(&tmp, object, xnode, NODE_NEXT));
		ZVAL_UNDEF(&tmp);
		add_assoc_zval(return_value, "enext", php_mecab_node_get_sibling(&tmp, object, xnode, NODE_ENEXT));
		ZVAL_UNDEF(&tmp);
		add_assoc_zval(return_value, "bnext", php_mecab_node_get_sibling(&tmp, object, xnode, NODE_BNEXT));
		ZVAL_UNDEF(&tmp);
		add_assoc_zval(return_value, "rpath", php_mecab_node_get_path(&tmp, object, xnode, NODE_RPATH));
		ZVAL_UNDEF(&tmp);
		add_assoc_zval(return_value, "lpath", php_mecab_node_get_path(&tmp, object, xnode, NODE_LPATH));
	}

	/* assign node info */
	add_assoc_stringl(return_value, "surface", (char *)node->surface, (int)node->length);
	add_assoc_stringl(return_value, "feature", (char *)node->feature, (int)strlen(node->feature));
	add_assoc_long(return_value, "id",         (zend_long)node->id);
	add_assoc_long(return_value, "length",     (zend_long)node->length);
	add_assoc_long(return_value, "rlength",    (zend_long)node->rlength);
	add_assoc_long(return_value, "rcAttr",     (zend_long)node->rcAttr);
	add_assoc_long(return_value, "lcAttr",     (zend_long)node->lcAttr);
	add_assoc_long(return_value, "posid",      (zend_long)node->posid);
	add_assoc_long(return_value, "char_type",  (zend_long)node->char_type);
	add_assoc_long(return_value, "stat",       (zend_long)node->stat);
	add_assoc_bool(return_value, "isbest",     (zend_long)node->isbest);
	add_assoc_double(return_value, "alpha", (double)node->alpha);
	add_assoc_double(return_value, "beta",  (double)node->beta);
	add_assoc_double(return_value, "prob",  (double)node->prob);
	add_assoc_long(return_value,   "wcost", (zend_long)node->wcost);
	add_assoc_long(return_value,   "cost",  (zend_long)node->cost);
}
/* }}} mecab_node_toarray */

/* {{{ proto string mecab_node_tostring(resource mecab_node node) */
/**
 * string mecab_node_tostring(resource mecab_node node)
 * string MeCab_Node::toString(void)
 * string MeCab_Node::__toString(void)
 *
 * Get the formatted string of the node.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	string	The formatted string.
 * @see	mecab_format_node
 */
PHP_METHOD(MeCab_Node, __toString)
{
	/* declaration of the resources */
	php_mecab_node *xnode = NULL;
	const mecab_node_t *node = NULL;

	/* local variables */
	mecab_t *mecab = NULL;
	const char *fmt = NULL;

	/* parse the arguments */
	PHP_MECAB_NODE_FROM_PARAMETER();

	/* call mecab_format_node() */
	mecab = xnode->tagger->ptr;
	fmt = mecab_format_node(mecab, node);
	if (fmt == NULL) {
		php_error_docref(NULL, E_WARNING, "%s", mecab_strerror(mecab));
		RETURN_FALSE;
	}

	/* set return value */
	RETURN_STRING((char *)fmt);
}
/* }}} mecab_node_tostring */

/* {{{ proto resource mecab_node mecab_node_prev(resource mecab_node node) */
/**
 * resource mecab_node mecab_node_prev(resource mecab_node node)
 * object MeCab_Node MeCab_Node::getPrev(void)
 *
 * Get the previous node.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	resource mecab_node	The previous node.
 *								If the given node is the first one, returns FALSE.
 */
PHP_METHOD(MeCab_Node, getPrev)
{
	php_mecab_node_get_sibling_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, NODE_PREV);
}
/* }}} mecab_node_prev */

/* {{{ proto resource mecab_node mecab_node_next(resource mecab_node node) */
/**
 * resource mecab_node mecab_node_next(resource mecab_node node)
 * object MeCab_Node MeCab_Node::getNext(void)
 *
 * Get the next node.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	resource mecab_node	The next node.
 *								If the given node is the last one, returns FALSE.
 */
PHP_METHOD(MeCab_Node, getNext)
{
	php_mecab_node_get_sibling_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, NODE_NEXT);
}
/* }}} mecab_node_next */

/* {{{ proto resource mecab_node mecab_node_enext(resource mecab_node node) */
/**
 * resource mecab_node mecab_node_enext(resource mecab_node node)
 * object MeCab_Node MeCab_Node::getENext(void)
 *
 * Get the enext node.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	resource mecab_node	The next node which has same end point as the given node.
 *								If there is no `enext' node, returns false.
 */
PHP_METHOD(MeCab_Node, getENext)
{
	php_mecab_node_get_sibling_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, NODE_ENEXT);
}
/* }}} mecab_node_enext */

/* {{{ proto resource mecab_node mecab_node_bnext(resource mecab_node node) */
/**
 * resource mecab_node mecab_node_bnext(resource mecab_node node)
 * object MeCab_Node MeCab_Node::getBNext(void)
 *
 * Get the bnext node.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	resource mecab_node	The next node which has same beggining point as the given one.
 *								If there is no `bnext' node, returns false.
 */
PHP_METHOD(MeCab_Node, getBNext)
{
	php_mecab_node_get_sibling_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, NODE_BNEXT);
}
/* }}} mecab_node_bnext */

/* {{{ proto resource mecab_path mecab_node_rpath(resource mecab_node node) */
/**
 * resource mecab_path mecab_node_rpath(resource mecab_node node)
 * object MeCab_Path MeCab_Node::getRPath(void)
 *
 * Get the rpath.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	resource mecab_path	The next node which has same end point as the given node.
 *								If there is no `rpath' node, returns false.
 */
PHP_METHOD(MeCab_Node, getRPath)
{
	php_mecab_node_get_path_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, NODE_RPATH);
}
/* }}} mecab_node_rpath */

/* {{{ proto resource mecab_path mecab_node_lpath(resource mecab_node node) */
/**
 * resource mecab_path mecab_node_lpath(resource mecab_node node)
 * object MeCab_Path MeCab_Node::getLPath(void)
 *
 * Get the lpath.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	resource mecab_path	The next node which has same beggining point as the given one.
 *								If there is no `lpath' node, returns false.
 */
PHP_METHOD(MeCab_Node, getLPath)
{
	php_mecab_node_get_path_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, NODE_LPATH);
}
/* }}} mecab_node_lpath */

/* {{{ proto string mecab_node_surface(resource mecab_node node) */
/**
 * string mecab_node_surface(resource mecab_node node)
 * string MeCab_Node::getSurface(void)
 *
 * Get the surface.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	string	The surface of the node.
 */
PHP_METHOD(MeCab_Node, getSurface)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(STRINGL, (char *)node->surface, (int)node->length);
}
/* }}} mecab_node_surface */

/* {{{ proto string mecab_node_feature(resource mecab_node node) */
/**
 * string mecab_node_feature(resource mecab_node node)
 * string MeCab_Node::getFeature(void)
 *
 * Get the feature.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	string	The feature of the node.
 */
PHP_METHOD(MeCab_Node, getFeature)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(STRING, (char *)node->feature);
}
/* }}} mecab_node_feature */

/* {{{ proto int mecab_node_id(resource mecab_node node) */
/**
 * int mecab_node_id(resource mecab_node node)
 * int MeCab_Node::getId(void)
 *
 * Get the ID.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The ID of the node.
 */
PHP_METHOD(MeCab_Node, getId)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->id);
}
/* }}} mecab_node_id */

/* {{{ proto int mecab_node_length(resource mecab_node node) */
/**
 * int mecab_node_length(resource mecab_node node)
 * int MeCab_Node::getLength(void)
 *
 * Get the length of the surface.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The length of the surface of the node.
 */
PHP_METHOD(MeCab_Node, getLength)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->length);
}
/* }}} mecab_node_length */

/* {{{ proto int mecab_node_rlength(resource mecab_node node) */
/**
 * int mecab_node_rlength(resource mecab_node node)
 * int MeCab_Node::getRLength(void)
 *
 * Get the length of the surface and its leading whitespace.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The length of the surface and its leading whitespace of the node.
 */
PHP_METHOD(MeCab_Node, getRLength)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->rlength);
}
/* }}} mecab_node_rlength */

/* {{{ proto int mecab_node_rcattr(resource mecab_node node) */
/**
 * int mecab_node_rcattr(resource mecab_node node)
 * int MeCab_Node::getRcAttr(void)
 *
 * Get the ID of the right context.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The ID of the right context.
 */
PHP_METHOD(MeCab_Node, getRcAttr)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->rcAttr);
}
/* }}} mecab_node_rcattr */

/* {{{ proto int mecab_node_lcattr(resource mecab_node node) */
/**
 * int mecab_node_lcattr(resource mecab_node node)
 * int MeCab_Node::getLcAttr(void)
 *
 * Get the ID of the left context.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The ID of the left context.
 */
PHP_METHOD(MeCab_Node, getLcAttr)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->lcAttr);
}
/* }}} mecab_node_lcattr */

/* {{{ proto int mecab_node_posid(resource mecab_node node) */
/**
 * int mecab_node_posid(resource mecab_node node)
 * int MeCab_Node::getPosId(void)
 *
 * Get the ID of the Part-of-Speech.
 * (node->posid is not used in MeCab-0.90)
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The ID of the Part-of-Speech.
 *				Currently, always returns 0.
 */
PHP_METHOD(MeCab_Node, getPosId)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->posid);
}
/* }}} mecab_node_posid */

/* {{{ proto int mecab_node_char_type(resource mecab_node node) */
/**
 * int mecab_node_char_type(resource mecab_node node)
 * int MeCab_Node::getCharType(void)
 *
 * Get the type of the character.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The type of the character.
 */
PHP_METHOD(MeCab_Node, getCharType)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->char_type);
}
/* }}} mecab_node_char_type */

/* {{{ proto int mecab_node_stat(resource mecab_node node) */
/**
 * int mecab_node_stat(resource mecab_node node)
 * int MeCab_Node::getStat(void)
 *
 * Get the status.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The status of the node.
 *				The return value is one of the following:
 *					MECAB_NOR_NODE (0:Normal)
 *					MECAB_UNK_NODE (1:Unknown)
 *					MECAB_BOS_NODE (2:Beginning-of-Sentence)
 *					MECAB_EOS_NODE (3:End-of-Sentence)
 */
PHP_METHOD(MeCab_Node, getStat)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->stat);
}
/* }}} mecab_node_stat */

/* {{{ proto bool mecab_node_isbest(resource mecab_node node) */
/**
 * bool mecab_node_isbest(resource mecab_node node)
 * bool MeCab_Node::isBest(void)
 *
 * Determine whether the node is the best solution.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	bool	True if the node is the best, otherwise returns false.
 */
PHP_METHOD(MeCab_Node, isBest)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(BOOL, (node->isbest == 1));
}
/* }}} mecab_node_isbest */

/* {{{ proto double mecab_node_alpha(resource mecab_node node) */
/**
 * double mecab_node_alpha(resource mecab_node node)
 * double MeCab_Node::getAlpha(void)
 *
 * Get the forward log probability.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	double	The forward log probability of the node.
 */
PHP_METHOD(MeCab_Node, getAlpha)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(DOUBLE, (double)node->alpha);
}
/* }}} mecab_node_alpha */

/* {{{ proto double mecab_node_beta(resource mecab_node node) */
/**
 * double mecab_node_beta(resource mecab_node node)
 * double MeCab_Node::getBeta(void)
 *
 * Get the backward log probability.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	double	The backward log probability of the node.
 */
PHP_METHOD(MeCab_Node, getBeta)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(DOUBLE, (double)node->beta);
}
/* }}} mecab_node_beta */

/* {{{ proto double mecab_node_prob(resource mecab_node node) */
/**
 * double mecab_node_prob(resource mecab_node node)
 * double MeCab_Node::getProb(void)
 *
 * Get the marginal probability.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	double	The marginal probability of the node.
 */
PHP_METHOD(MeCab_Node, getProb)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(DOUBLE, (double)node->prob);
}
/* }}} mecab_node_prob */

/* {{{ proto int mecab_node_wcost(resource mecab_node node) */
/**
 * int mecab_node_wcost(resource mecab_node node)
 * int MeCab_Node::getWCost(void)
 *
 * Get the word arising cost.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The word arising cost of the node.
 */
PHP_METHOD(MeCab_Node, getWCost)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->wcost);
}
/* }}} mecab_node_wcost */

/* {{{ proto int mecab_node_cost(resource mecab_node node) */
/**
 * int mecab_node_cost(resource mecab_node node)
 * int MeCab_Node::getCost(void)
 *
 * Get the cumulative cost.
 *
 * @param	resource mecab_node	$node	The node of the source string.
 * @return	int	The cumulative cost of the node.
 */
PHP_METHOD(MeCab_Node, getCost)
{
	PHP_MECAB_NODE_RETURN_PROPERTY(LONG, (zend_long)node->cost);
}
/* }}} mecab_node_cost */

/* {{{ proto resource mecab_path mecab_path_rnext(resource mecab_path path) */
/**
 * resource mecab_path mecab_path_rnext(resource mecab_path path)
 * object MeCab_Path MeCab_Path::getRNext(void)
 *
 * Get the rnext path.
 *
 * @param	resource mecab_path	$path	The path of the source string.
 * @return	resource mecab_path	The rnext path.
 *								If the given path is the first one, returns FALSE.
 */
PHP_METHOD(MeCab_Path, getRNext)
{
	php_mecab_path_get_sibling_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PATH_RNEXT);
}
/* }}} mecab_path_rnext */

/* {{{ proto resource mecab_path mecab_path_lnext(resource mecab_path path) */
/**
 * resource mecab_path mecab_path_lnext(resource mecab_path path)
 * object MeCab_Path MeCab_Path::getLNext(void)
 *
 * Get the lnext path.
 *
 * @param	resource mecab_path	$path	The path of the source string.
 * @return	resource mecab_path	The lnext path.
 *								If the given path is the last one, returns FALSE.
 */
PHP_METHOD(MeCab_Path, getLNext)
{
	php_mecab_path_get_sibling_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PATH_LNEXT);
}
/* }}} mecab_path_lnext */

/* {{{ proto resource mecab_node mecab_path_rnode(resource mecab_path path) */
/**
 * resource mecab_node mecab_path_rnode(resource mecab_path path)
 * object MeCab_Node MeCab_Path::getRNode(void)
 *
 * Get the rnode.
 *
 * @param	resource mecab_path	$path	The path of the source string.
 * @return	resource mecab_node	The next path which has same end point as the given path.
 *								If there is no `rnode' path, returns false.
 */
PHP_METHOD(MeCab_Path, getRNode)
{
	php_mecab_path_get_node_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PATH_RNODE);
}
/* }}} mecab_path_rnode */

/* {{{ proto resource mecab_node mecab_path_lnode(resource mecab_path path) */
/**
 * resource mecab_node mecab_path_lnode(resource mecab_path path)
 * object MeCab_Node MeCab_Path::getLNode(void)
 *
 * Get the lnode.
 *
 * @param	resource mecab_path	$path	The path of the source string.
 * @return	resource mecab_node	The next path which has same beggining point as the given one.
 *								If there is no `lnode' path, returns false.
 */
PHP_METHOD(MeCab_Path, getLNode)
{
	php_mecab_path_get_node_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, PATH_LNODE);
}
/* }}} mecab_path_lnode */

/* {{{ proto double mecab_path_prob(resource mecab_path path) */
/**
 * double mecab_path_prob(resource mecab_path path)
 * double MeCab_Path::getProb(void)
 *
 * Get the marginal probability.
 *
 * @param	resource mecab_path	$path	The path of the source string.
 * @return	double	The marginal probability of the path.
 */
PHP_METHOD(MeCab_Path, getProb)
{
	PHP_MECAB_PATH_RETURN_PROPERTY(DOUBLE, (double)(path->prob));
}
/* }}} mecab_path_prob */

/* {{{ proto int mecab_path_cost(resource mecab_path path) */
/**
 * int mecab_path_cost(resource mecab_path path)
 * int MeCab_Path::getCost(void)
 *
 * Get the cumulative cost.
 *
 * @param	resource mecab_path	$path	The path of the source string.
 * @return	int	The cumulative cost of the path.
 */
PHP_METHOD(MeCab_Path, getCost)
{
	PHP_MECAB_PATH_RETURN_PROPERTY(LONG, (zend_long)(path->cost));
}
/* }}} mecab_node_cost */

/* }}} Functions */

/* {{{ methods */

/* {{{ methods of class MeCab_Node*/

/* {{{ proto object MeCab_Node __construct(void) */
/**
 * object MeCab_Node MeCab_Node::__construct(void)
 *
 * Create MeCab_Node object.
 *
 * @access	private
 * @igore
 */
PHP_METHOD(MeCab_Node, __construct)
{
	return;
}
/* }}} MeCab_Node::__construct */

/* {{{ proto mixed MeCab_Node::__get(string name) */
/**
 * mixed MeCab_Node::__get(string name)
 *
 * [Overloading implementation]
 * A magick getter.
 *
 * @param	string	$name	The name of property.
 * @return	mixed	The value of the property.
 *					If there is not a named property, causes E_NOTICE error and returns false.
 * @access	public
 * @ignore
 */
PHP_METHOD(MeCab_Node, __get)
{
	/* declaration of the resources */
	zval *object = ZEND_THIS;
	php_mecab_node *xnode = NULL;
	const mecab_node_t *node = NULL;

	/* declaration of the arguments */
	zend_string *zname = NULL;
	const char *name = NULL;
	size_t len;

	/* parse the arguments */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &zname) == FAILURE) {
		return;
	} else {
		const php_mecab_node_object *intern = PHP_MECAB_NODE_OBJECT_P(object);
		xnode = intern->ptr;
		node = xnode->ptr;
		name = ZSTR_VAL(zname);
		len = ZSTR_LEN(zname);
	}

	/* check for given property name */
	if (!zend_binary_strcmp(name, len, ZEND_STRL("prev"))) {
		php_mecab_node_get_sibling(return_value, object, xnode, NODE_PREV);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("next"))) {
		php_mecab_node_get_sibling(return_value, object, xnode, NODE_NEXT);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("enext"))) {
		php_mecab_node_get_sibling(return_value, object, xnode, NODE_ENEXT);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("bnext"))) {
		php_mecab_node_get_sibling(return_value, object, xnode, NODE_BNEXT);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("rpath"))) {
		php_mecab_node_get_path(return_value, object, xnode, NODE_RPATH);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("lpath"))) {
		php_mecab_node_get_path(return_value, object, xnode, NODE_LPATH);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("surface")))   RETURN_STRINGL((char *)node->surface, (int)node->length);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("feature")))   RETURN_STRING((char *)node->feature);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("id")))        RETURN_LONG((zend_long)node->id);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("length")))    RETURN_LONG((zend_long)node->length);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("rlength")))   RETURN_LONG((zend_long)node->rlength);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("rcAttr")))    RETURN_LONG((zend_long)node->rcAttr);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("lcAttr")))    RETURN_LONG((zend_long)node->lcAttr);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("posid")))     RETURN_LONG((zend_long)node->posid);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("char_type"))) RETURN_LONG((zend_long)node->char_type);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("stat")))      RETURN_LONG((zend_long)node->stat);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("isbest")))    RETURN_BOOL((bool)node->isbest);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("alpha")))     RETURN_DOUBLE((double)node->alpha);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("beta")))      RETURN_DOUBLE((double)node->beta);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("prob")))      RETURN_DOUBLE((double)node->prob);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("wcost")))     RETURN_LONG((zend_long)node->wcost);
	if (!zend_binary_strcmp(name, len, ZEND_STRL("cost")))      RETURN_LONG((zend_long)node->cost);

	/* when going to fetch undefined property */
	php_error_docref(NULL, E_NOTICE, "Undefined property (%s)", name);
	RETURN_NULL();
}
/* }}} MeCab_Node::__get */

/* {{{ proto bool MeCab_Node::__isset(string name) */
/**
 * bool MeCab_Node::__isset(string name)
 *
 * [Overloading implementation]
 * Determine whether there is a named property.
 *
 * @param	string	$name	The name of property.
 * @return	bool	True if there is a named property, otherwise returns false.
 * @access	public
 * @ignore
 */
PHP_METHOD(MeCab_Node, __isset)
{
	/* declaration of the resources */
	php_mecab_node *xnode = NULL;
	const mecab_node_t *node = NULL;

	/* declaration of the arguments */
	zend_string *zname = NULL;
	const char *name = NULL;
	size_t len;

	/* parse the arguments */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &zname) == FAILURE) {
		return;
	} else {
		const php_mecab_node_object *intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);
		xnode = intern->ptr;
		node = xnode->ptr;
		name = ZSTR_VAL(zname);
		len = ZSTR_LEN(zname);
	}

	/* check for given property name */
	if ((!zend_binary_strcmp(name, len, ZEND_STRL("prev")) && node->prev != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("next")) && node->next != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("enext")) && node->enext != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("bnext")) && node->bnext != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("rpath")) && node->rpath != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("lpath")) && node->lpath != NULL) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("surface")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("feature")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("id")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("length")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("rlength")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("rcAttr")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("lcAttr")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("posid")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("char_type")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("stat")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("isbest")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("sentence_length")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("alpha")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("beta")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("prob")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("wcost")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("cost")))
	{
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} MeCab_Node::__isset */

/* {{{ proto object MeCab_NodeIterator MeCab_Node::getIterator(void) */
/**
 * object MeCab_NodeIterator MeCab_Node::getIterator(void)
 *
 * [IteratorAggregate implementation]
 * Return the iterator object.
 *
 * @return	object MeCab_NodeIterator
 * @access	public
 * @ignore
 */
PHP_METHOD(MeCab_Node, getIterator)
{
	php_mecab_node_object *intern;
	php_mecab_node *xnode;
	const mecab_node_t *node;
	php_mecab_node_object *newobj;
	php_mecab_node *newnode;

	intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);
	xnode = intern->ptr;
	node = xnode->ptr;

	if (node == NULL) {
		RETURN_NULL();
	}

	object_init_ex(return_value, ce_MeCab_NodeIterator);
	newobj = PHP_MECAB_NODE_OBJECT_P(return_value);
	newobj->root = node;
	newobj->mode = intern->mode;
	newnode = newobj->ptr;
	newnode->ptr = node;
	php_mecab_node_set_tagger(newnode, xnode->tagger);
}
/* }}} MeCab_Node::getIterator */

/* {{{ proto void MeCab_Node::setTraverse(int tranverse) */
/**
 * void MeCab_Node::setTraverse(int tranverse)
 *
 * Set the traverse mode.
 *
 * @param	int	$traverse	The traverse mode.
 * @return	void
 * @throws	InvalidArgumentException
 * @access	public
 */
PHP_METHOD(MeCab_Node, setTraverse)
{
	php_mecab_node_object *intern;
	zend_long traverse = 0;

#if PHP_VERSION_ID >= 50300
	zend_error_handling error_handling;

	zend_replace_error_handling(EH_THROW, spl_ce_InvalidArgumentException, &error_handling);
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &traverse) == FAILURE) {
		zend_restore_error_handling(&error_handling);
		return;
	}
	zend_restore_error_handling(&error_handling);
#else
	php_set_error_handling(EH_THROW, spl_ce_InvalidArgumentException);
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &traverse) == FAILURE) {
		php_std_error_handling();
		return;
	}
	php_std_error_handling();
#endif

	intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);

	if (traverse == (zend_long)TRAVERSE_NEXT ||
		traverse == (zend_long)TRAVERSE_ENEXT ||
		traverse == (zend_long)TRAVERSE_BNEXT)
	{
		intern->mode = (php_mecab_traverse_mode)traverse;
	} else {
		zend_throw_exception(spl_ce_InvalidArgumentException,
				"Invalid traverse mdoe given", 0);
	}
}
/* }}} MeCab_Node::setTraverse */

/* }}} methods of class MeCab_Node */

/* {{{ methods of class MeCab_NodeIterator*/

/* {{{ proto object MeCab_NodeIterator __construct(void) */
/**
 * object MeCab_Path MeCab_NodeIterator::__construct(void)
 *
 * Create MeCab_NodeIterator object.
 *
 * @access	private
 * @igore
 */
PHP_METHOD(MeCab_NodeIterator, __construct)
{
	return;
}
/* }}} MeCab_NodeIterator::__construct */

/* {{{ object MeCab_Node MeCab_NodeIterator::current(void) */
/**
 * object MeCab_Node MeCab_NodeIterator::current(void)
 *
 * [Iterator implementation]
 * Return the current element.
 *
 * @access	public
 * @igore
 */
PHP_METHOD(MeCab_NodeIterator, current)
{
	php_mecab_node_object *intern;
	php_mecab_node *xnode;
	const mecab_node_t *node;
	php_mecab_node_object *newobj;
	php_mecab_node *newnode;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);
	xnode = intern->ptr;
	node = xnode->ptr;

	if (node == NULL) {
		RETURN_NULL();
	}

	object_init_ex(return_value, ce_MeCab_Node);
	newobj = PHP_MECAB_NODE_OBJECT_P(return_value);
	newobj->mode = intern->mode;
	newnode = newobj->ptr;
	newnode->ptr = node;
	php_mecab_node_set_tagger(newnode, xnode->tagger);
}
/* }}} MeCab_NodeIterator::current */

/* {{{ proto int MeCab_NodeIterator::key(void) */
/**
 * int MeCab_Node::key(void)
 *
 * [Iterator implementation]
 * Return the key of the current element.
 *
 * @return	int	The cumulative cost of the node.
 * @access	public
 * @igore
 */
PHP_METHOD(MeCab_NodeIterator, key)
{
	php_mecab_node_object *intern;
	php_mecab_node *xnode;
	const mecab_node_t *node;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);
	xnode = intern->ptr;
	node = xnode->ptr;

	if (node == NULL) {
		RETURN_NULL();
	}

	RETURN_LONG((zend_long)node->id);
}
/* }}} MeCab_NodeIterator::key */

/* {{{ proto void MeCab_NodeIterator::next(void) */
/**
 * void MeCab_NodeIterator::next(void)
 *
 * [Iterator implementation]
 * Set the node pointer to the next.
 *
 * @return	void
 * @access	public
 * @igore
 */
PHP_METHOD(MeCab_NodeIterator, next)
{
	php_mecab_node_object *intern;
	php_mecab_node *xnode;
	const mecab_node_t *node;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);
	xnode = intern->ptr;
	node = xnode->ptr;

	if (node == NULL) {
		return;
	}

	switch (intern->mode) {
	  case TRAVERSE_NEXT:
		xnode->ptr = node->next;
		break;
	  case TRAVERSE_ENEXT:
		xnode->ptr = node->enext;
		break;
	  case TRAVERSE_BNEXT:
		xnode->ptr = node->bnext;
		break;
	  default:
		xnode->ptr = NULL;
	}
}
/* }}} MeCab_NodeIterator::next */

/* {{{ proto void MeCab_NodeIterator::rewind(void) */
/**
 * void MeCab_NodeIterator::rewind(void)
 *
 * [Iterator implementation]
 * Set the node pointer to the beginning.
 *
 * @return	void
 * @access	public
 * @igore
 */
PHP_METHOD(MeCab_NodeIterator, rewind)
{
	php_mecab_node_object *intern;
	php_mecab_node *xnode;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);
	xnode = intern->ptr;
	xnode->ptr = intern->root;
}
/* }}} MeCab_NodeIterator::rewind */

/* {{{ proto bool MeCab_NodeIterator::valid(void) */
/**
 * bool MeCab_NodeIterator::valid(void)
 *
 * [Iterator implementation]
 * Check if there is a current element after calls to rewind() or next().
 *
 * @return	bool	True if there is an element after the current element, otherwise returns false.
 * @access	public
 * @igore
 */
PHP_METHOD(MeCab_NodeIterator, valid)
{
	php_mecab_node_object *intern;
	php_mecab_node *xnode;
	const mecab_node_t *node;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	intern = PHP_MECAB_NODE_OBJECT_P(ZEND_THIS);
	xnode = intern->ptr;
	node = xnode->ptr;

	RETURN_BOOL(node != NULL);
}
/* }}} MeCab_NodeIterator::valid */

/* }}} methods of class MeCab_NodeIterator */

/* {{{ methods of class MeCab_Path*/

/* {{{ proto object MeCab_Path __construct(void) */
/**
 * object MeCab_Path MeCab_Path::__construct(void)
 *
 * Create MeCab_Path object.
 *
 * @access	private
 * @igore
 */
PHP_METHOD(MeCab_Path, __construct)
{
	return;
}
/* }}} MeCab_Path::__construct */

/* {{{ proto mixed MeCab_Path::__get(string name) */
/**
 * mixed MeCab_Path::__get(string name)
 *
 * [Overloading implementation]
 * A magick getter.
 *
 * @param	string	$name	The name of property.
 * @return	mixed	The value of the property.
 *					If there is not a named property, causes E_NOTICE error and returns false.
 * @access	public
 * @ignore
 */
PHP_METHOD(MeCab_Path, __get)
{
	/* declaration of the resources */
	zval *object = ZEND_THIS;
	php_mecab_path *xpath = NULL;
	const mecab_path_t *path = NULL;

	/* declaration of the arguments */
	zend_string *zname = NULL;
	const char *name = NULL;
	size_t len;

	/* parse the arguments */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &zname) == FAILURE) {
		return;
	} else {
		const php_mecab_path_object *intern = PHP_MECAB_PATH_OBJECT_P(object);
		xpath = intern->ptr;
		path = xpath->ptr;
		name = ZSTR_VAL(zname);
		len = ZSTR_LEN(zname);
	}

	/* check for given property name */
	if (!zend_binary_strcmp(name, len, ZEND_STRL("rnext"))) {
		php_mecab_path_get_sibling(return_value, object, xpath, PATH_RNEXT);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("lnext"))) {
		php_mecab_path_get_sibling(return_value, object, xpath, PATH_LNEXT);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("rnode"))) {
		php_mecab_path_get_node(return_value, object, xpath, PATH_RNODE);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("lnode"))) {
		php_mecab_path_get_node(return_value, object, xpath, PATH_LNODE);
		return;
	}
	if (!zend_binary_strcmp(name, len, ZEND_STRL("prob"))) RETURN_DOUBLE((double)(path->prob));
	if (!zend_binary_strcmp(name, len, ZEND_STRL("cost"))) RETURN_LONG((zend_long)(path->cost));

	/* when going to fetch undefined property */
	php_error_docref(NULL, E_NOTICE, "Undefined property (%s)", name);
	RETURN_NULL();
}
/* }}} MeCab_Path::__get */

/* {{{ proto boolMeCab_Path:: __isset(string name) */
/**
 * bool MeCab_Path::__isset(string name)
 *
 * [Overloading implementation]
 * Determine whether there is a named property.
 *
 * @param	string	$name	The name of property.
 * @return	bool	True if there is a named property, otherwise returns false.
 * @access	public
 * @ignore
 */
PHP_METHOD(MeCab_Path, __isset)
{
	/* declaration of the resources */
	php_mecab_path *xpath = NULL;
	const mecab_path_t *path = NULL;

	/* declaration of the arguments */
	zend_string *zname = NULL;
	const char *name = NULL;
	size_t len;

	/* parse the arguments */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "S", &zname) == FAILURE) {
		return;
	} else {
		const php_mecab_path_object *intern = PHP_MECAB_PATH_OBJECT_P(ZEND_THIS);
		xpath = intern->ptr;
		path = xpath->ptr;
		name = ZSTR_VAL(zname);
		len = ZSTR_LEN(zname);
	}

	/* check for given property name */
	if ((!zend_binary_strcmp(name, len, ZEND_STRL("rnext")) && path->rnext != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("lnext")) && path->lnext != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("rnode")) && path->rnode != NULL) ||
		(!zend_binary_strcmp(name, len, ZEND_STRL("lnode")) && path->lnode != NULL) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("prob")) ||
		!zend_binary_strcmp(name, len, ZEND_STRL("cost")))
	{
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} MeCab_Path::__isset */

/* }}} methods of class MeCab_Path */

/* }}} methods */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
