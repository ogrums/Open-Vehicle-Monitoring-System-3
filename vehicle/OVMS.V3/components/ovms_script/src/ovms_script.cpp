/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "script";

#include <string>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <esp_task_wdt.h>
#include "ovms_malloc.h"
#include "ovms_module.h"
#include "ovms_script.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "console_async.h"
#include "buffered_shell.h"
#include "ovms_netmanager.h"

OvmsScripts MyScripts __attribute__ ((init_priority (1600)));

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

OvmsWriter* duktapewriter = NULL;

DuktapeObjectRegistration::DuktapeObjectRegistration(const char* name)
  {
  m_name = name;
  }

DuktapeObjectRegistration::~DuktapeObjectRegistration()
  {
  }

const char* DuktapeObjectRegistration::GetName()
  {
  return m_name;
  }

void DuktapeObjectRegistration::RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name)
  {
  duktape_registerfunction_t* fn = new duktape_registerfunction_t;
  fn->func = func;
  fn->nargs = nargs;
  m_fnmap[name] = fn;
  }

void DuktapeObjectRegistration::RegisterWithDuktape(duk_context* ctx)
  {
  duk_push_object(ctx);

  DuktapeFunctionMap::iterator itm=m_fnmap.begin();
  while (itm!=m_fnmap.end())
    {
    const char* name = itm->first;
    duktape_registerfunction_t* fn = itm->second;
    ESP_LOGD(TAG,"Duktape: Pre-Registered object %s function %s",m_name,name);
    duk_push_c_function(ctx, fn->func, fn->nargs);
    duk_push_string(ctx, "name");
    duk_push_string(ctx, name);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);  /* Improve stacktraces by displaying function name */
    duk_put_prop_string(ctx, -2, name);
    ++itm;
    }

  duk_put_global_string(ctx, m_name);
  }

static duk_int_t duk__eval_module_source(duk_context *ctx, void *udata);
static void duk__push_module_object(duk_context *ctx, const char *id, duk_bool_t main);

static duk_bool_t duk__get_cached_module(duk_context *ctx, const char *id)
  {
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	if (duk_get_prop_string(ctx, -1, id))
    {
		duk_remove(ctx, -2);
		duk_remove(ctx, -2);
		return 1;
  	}
  else
    {
		duk_pop_3(ctx);
		return 0;
	  }
  }

/* Place a `module` object on the top of the value stack into the require cache
 * based on its `.id` property.  As a convenience to the caller, leave the
 * object on top of the value stack afterwards.
 */
static void duk__put_cached_module(duk_context *ctx)
  {
	/* [ ... module ] */

	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_dup(ctx, -3);

	/* [ ... module stash req_cache module ] */

	(void) duk_get_prop_string(ctx, -1, "id");
	duk_dup(ctx, -2);
	duk_put_prop(ctx, -4);

	duk_pop_3(ctx);  /* [ ... module ] */
  }

static void duk__del_cached_module(duk_context *ctx, const char *id)
  {
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_del_prop_string(ctx, -1, id);
	duk_pop_2(ctx);
  }

static duk_ret_t duk__handle_require(duk_context *ctx)
  {
	/*
	 *  Value stack handling here is a bit sloppy but should be correct.
	 *  Call handling will clean up any extra garbage for us.
	 */

	const char *id;
	const char *parent_id;
	duk_idx_t module_idx;
	duk_idx_t stash_idx;
	duk_int_t ret;

	duk_push_global_stash(ctx);
	stash_idx = duk_normalize_index(ctx, -1);

	duk_push_current_function(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "moduleId");
	parent_id = duk_require_string(ctx, -1);
	(void) parent_id;  /* not used directly; suppress warning */

	/* [ id stash require parent_id ] */

	id = duk_require_string(ctx, 0);

	(void) duk_get_prop_string(ctx, stash_idx, "\xff" "modResolve");
	duk_dup(ctx, 0);   /* module ID */
	duk_dup(ctx, -3);  /* parent ID */
	duk_call(ctx, 2);

	/* [ ... stash ... resolved_id ] */

	id = duk_require_string(ctx, -1);

	if (duk__get_cached_module(ctx, id))
    {
		goto have_module;  /* use the cached module */
	  }

	duk__push_module_object(ctx, id, 0 /*main*/);
	duk__put_cached_module(ctx);  /* module remains on stack */

	/*
	 *  From here on out, we have to be careful not to throw.  If it can't be
	 *  avoided, the error must be caught and the module removed from the
	 *  require cache before rethrowing.  This allows the application to
	 *  reattempt loading the module.
	 */

	module_idx = duk_normalize_index(ctx, -1);

	/* [ ... stash ... resolved_id module ] */

	(void) duk_get_prop_string(ctx, stash_idx, "\xff" "modLoad");
	duk_dup(ctx, -3);  /* resolved ID */
	(void) duk_get_prop_string(ctx, module_idx, "exports");
	duk_dup(ctx, module_idx);
	ret = duk_pcall(ctx, 3);
	if (ret != DUK_EXEC_SUCCESS)
    {
		duk__del_cached_module(ctx, id);
		duk_throw(ctx);  /* rethrow */
	  }

	if (duk_is_string(ctx, -1))
    {
		duk_int_t ret;

		/* [ ... module source ] */

		ret = duk_safe_call(ctx, duk__eval_module_source, NULL, 2, 1);
		if (ret != DUK_EXEC_SUCCESS)
      {
			duk__del_cached_module(ctx, id);
			duk_throw(ctx);  /* rethrow */
		  }
	  }
  else if (duk_is_undefined(ctx, -1))
    {
		duk_pop(ctx);
  	}
  else
    {
		duk__del_cached_module(ctx, id);
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "invalid module load callback return value");
	  }

	/* fall through */

 have_module:
	/* [ ... module ] */

	(void) duk_get_prop_string(ctx, -1, "exports");
	return 1;
  }

static void duk__push_require_function(duk_context *ctx, const char *id)
  {
	duk_push_c_function(ctx, duk__handle_require, 1);
	duk_push_string(ctx, "name");
	duk_push_string(ctx, "require");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE);
	duk_push_string(ctx, id);
	duk_put_prop_string(ctx, -2, "\xff" "moduleId");

	/* require.cache */
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "requireCache");
	duk_put_prop_string(ctx, -3, "cache");
	duk_pop(ctx);

	/* require.main */
	duk_push_global_stash(ctx);
	(void) duk_get_prop_string(ctx, -1, "\xff" "mainModule");
	duk_put_prop_string(ctx, -3, "main");
	duk_pop(ctx);
  }

static void duk__push_module_object(duk_context *ctx, const char *id, duk_bool_t main)
  {
	duk_push_object(ctx);

	/* Set this as the main module, if requested */
	if (main)
    {
		duk_push_global_stash(ctx);
		duk_dup(ctx, -2);
		duk_put_prop_string(ctx, -2, "\xff" "mainModule");
		duk_pop(ctx);
	  }

	/* Node.js uses the canonicalized filename of a module for both module.id
	 * and module.filename.  We have no concept of a file system here, so just
	 * use the module ID for both values.
	 */
	duk_push_string(ctx, id);
	duk_dup(ctx, -1);
	duk_put_prop_string(ctx, -3, "filename");
	duk_put_prop_string(ctx, -2, "id");

	/* module.exports = {} */
	duk_push_object(ctx);
	duk_put_prop_string(ctx, -2, "exports");

	/* module.loaded = false */
	duk_push_false(ctx);
	duk_put_prop_string(ctx, -2, "loaded");

	/* module.require */
	duk__push_require_function(ctx, id);
	duk_put_prop_string(ctx, -2, "require");
  }

static duk_int_t duk__eval_module_source(duk_context *ctx, void *udata)
  {
	const char *src;

	/*
	 *  Stack: [ ... module source ]
	 */

	(void) udata;

	/* Wrap the module code in a function expression.  This is the simplest
	 * way to implement CommonJS closure semantics and matches the behavior of
	 * e.g. Node.js.
	 */
	duk_push_string(ctx, "(function(exports,require,module,__filename,__dirname){");
	src = duk_require_string(ctx, -2);
	duk_push_string(ctx, (src[0] == '#' && src[1] == '!') ? "//" : "");  /* Shebang support. */
	duk_dup(ctx, -3);  /* source */
	duk_push_string(ctx, "\n})");  /* Newline allows module last line to contain a // comment. */
	duk_concat(ctx, 4);

	/* [ ... module source func_src ] */

	(void) duk_get_prop_string(ctx, -3, "filename");
	duk_compile(ctx, DUK_COMPILE_EVAL);
	duk_call(ctx, 0);

	/* [ ... module source func ] */

	/* Set name for the wrapper function. */
	duk_push_string(ctx, "name");
	duk_push_string(ctx, "main");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);

	/* call the function wrapper */
	(void) duk_get_prop_string(ctx, -3, "exports");   /* exports */
	(void) duk_get_prop_string(ctx, -4, "require");   /* require */
	duk_dup(ctx, -5);                                 /* module */
	(void) duk_get_prop_string(ctx, -6, "filename");  /* __filename */
	duk_push_undefined(ctx);                          /* __dirname */
	duk_call(ctx, 5);

	/* [ ... module source result(ignore) ] */

	/* module.loaded = true */
	duk_push_true(ctx);
	duk_put_prop_string(ctx, -4, "loaded");

	/* [ ... module source retval ] */

	duk_pop_2(ctx);

	/* [ ... module ] */

	return 1;
  }

/* Load a module as the 'main' module. */
duk_ret_t duk_module_node_peval_main(duk_context *ctx, const char *path)
  {
	/*
	 *  Stack: [ ... source ]
	 */

	duk__push_module_object(ctx, path, 1 /*main*/);
	/* [ ... source module ] */

	duk_dup(ctx, 0);
	/* [ ... source module source ] */

	return duk_safe_call(ctx, duk__eval_module_source, NULL, 2, 1);
  }

void duk_module_node_init(duk_context *ctx)
  {
	/*
	 *  Stack: [ ... options ] => [ ... ]
	 */

	duk_idx_t options_idx;

	duk_require_object_coercible(ctx, -1);  /* error before setting up requireCache */
	options_idx = duk_require_normalize_index(ctx, -1);

	/* Initialize the require cache to a fresh object. */
	duk_push_global_stash(ctx);
	duk_push_bare_object(ctx);
	duk_put_prop_string(ctx, -2, "\xff" "requireCache");
	duk_pop(ctx);

	/* Stash callbacks for later use.  User code can overwrite them later
	 * on directly by accessing the global stash.
	 */
	duk_push_global_stash(ctx);
	duk_get_prop_string(ctx, options_idx, "resolve");
	duk_require_function(ctx, -1);
	duk_put_prop_string(ctx, -2, "\xff" "modResolve");
	duk_get_prop_string(ctx, options_idx, "load");
	duk_require_function(ctx, -1);
	duk_put_prop_string(ctx, -2, "\xff" "modLoad");
	duk_pop(ctx);

	/* Stash main module. */
	duk_push_global_stash(ctx);
	duk_push_undefined(ctx);
	duk_put_prop_string(ctx, -2, "\xff" "mainModule");
	duk_pop(ctx);

	/* register `require` as a global function. */
	duk_push_global_object(ctx);
	duk_push_string(ctx, "require");
	duk__push_require_function(ctx, "");
	duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE |
	                      DUK_DEFPROP_SET_WRITABLE |
	                      DUK_DEFPROP_SET_CONFIGURABLE);
	duk_pop(ctx);

	duk_pop(ctx);  /* pop argument */
  }

void DukTapeLaunchTask(void *pvParameters)
  {
  OvmsScripts* me = (OvmsScripts*)pvParameters;

  me->DukTapeTask();
  }

void* DukOvmsAlloc(void *udata, duk_size_t size)
  {
  return ExternalRamMalloc(size);
  }

void* DukOvmsRealloc(void *udata, void *ptr, duk_size_t size)
  {
  return ExternalRamRealloc(ptr, size);
  }

void DukOvmsFree(void *udata, void *ptr)
  {
  free(ptr);
  }

void DukOvmsFatalHandler(void *udata, const char *msg)
  {
  ESP_LOGE(TAG, "Duktape fatal error: %s",msg);
  abort();
  }

static duk_ret_t DukOvmsResolveModule(duk_context *ctx)
  {
  const char *module_id;
  const char *parent_id;

  module_id = duk_require_string(ctx, 0);
  parent_id = duk_require_string(ctx, 1);

  duk_push_sprintf(ctx, "%s.js", module_id);
  ESP_LOGD(TAG,"resolve_cb: id:'%s', parent-id:'%s', resolve-to:'%s'",
                  module_id, parent_id, duk_get_string(ctx, -1));
  return 1;
  }

static duk_ret_t DukOvmsLoadModule(duk_context *ctx)
  {
  const char *filename;
  const char *module_id;

  module_id = duk_require_string(ctx, 0);
  duk_get_prop_string(ctx, 2, "filename");
  filename = duk_require_string(ctx, -1);

  if (strncmp(module_id,"int/",4)==0)
    {
    // Load internal module
    std::string name(module_id+4,strlen(module_id)-7);
    duktape_registermodule_t* mod = MyScripts.FindDuktapeModule(name.c_str());
    if (mod == NULL)
      {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "load_cb: cannot find internal module: %s", module_id);
      return 0;
      }
    else
      {
      ESP_LOGD(TAG,"load_cb: id:'%s' (internally provided %d bytes)", module_id, mod->length);
      duk_push_lstring(ctx, mod->start, mod->length);
      return 1;
      }
    }

  ESP_LOGD(TAG,"load_cb: id:'%s', filename:'%s'", module_id, filename);

  std::string path = std::string("/store/scripts/");
  path.append(filename);
  FILE* sf = fopen(path.c_str(), "r");
  if (sf == NULL)
    {
    path = std::string("/sd/scripts/");
    path.append(filename);
    sf = fopen(path.c_str(), "r");
    }
  if (sf == NULL)
    {
    duk_error(ctx, DUK_ERR_TYPE_ERROR, "load_cb: cannot find module: %s", module_id);
    return 0;
    }
  else
    {
    fseek(sf,0,SEEK_END);
    long slen = ftell(sf);
    fseek(sf,0,SEEK_SET);
    char *script = new char[slen+1];
    memset(script,0,slen+1);
    fread(script,1,slen,sf);
    duk_push_string(ctx, script);
    delete [] script;
    fclose(sf);
    }

  return 1;
  }

static duk_ret_t DukOvmsPrint(duk_context *ctx)
  {
  const char *output = duk_safe_to_string(ctx,0);

  if (output != NULL)
    {
    if (duktapewriter != NULL)
      { duktapewriter->printf("%s",output); }
    else
      { ESP_LOGI(TAG,"%s",output); }
    }
  return 0;
  }

static duk_ret_t DukOvmsAssert(duk_context *ctx)
  {
  if (duk_to_boolean(ctx, 0))
    {
    return 0;
    }
  duk_error(ctx, DUK_ERR_ERROR, "assertion failed: %s", duk_safe_to_string(ctx, 1));
  return 0;
  }

static duk_ret_t DukOvmsCommand(duk_context *ctx)
  {
  const char *cmd = duk_to_string(ctx,0);

  if (cmd != NULL)
    {
    BufferedShell* bs = new BufferedShell(false, COMMAND_RESULT_NORMAL);
    bs->SetSecure(true); // this is an authorized channel
    bs->ProcessChars(cmd, strlen(cmd));
    bs->ProcessChar('\n');
    std::string val; bs->Dump(val);
    delete bs;
    duk_push_string(ctx, val.c_str());
    return 1;  /* one return value */
    }
  else
    {
    return 0;
    }
  }

static duk_ret_t DukOvmsRaiseEvent(duk_context *ctx)
  {
  const char *event = duk_to_string(ctx,0);
  uint32_t delay_ms = duk_is_number(ctx,1) ? duk_to_uint32(ctx,1) : 0;

  if (event != NULL)
    {
    MyEvents.SignalEvent(event, NULL, (size_t)0, delay_ms);
    }
  return 0;  /* no return value */
  }

static duk_ret_t DukOvmsConfigParams(duk_context *ctx)
  {
  if (!MyConfig.ismounted()) return 0;

  duk_idx_t arr_idx = duk_push_array(ctx);
  int count = 0;
  for (ConfigMap::iterator it=MyConfig.m_map.begin(); it!=MyConfig.m_map.end(); ++it)
    {
    duk_push_string(ctx, it->first.c_str());
    duk_put_prop_index(ctx, arr_idx, count++);
    }

  return 1;
  }

static duk_ret_t DukOvmsConfigInstances(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);

  if (!MyConfig.ismounted()) return 0;

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Readable()) return 0;  // Parameter is protected, and not readable

    duk_idx_t arr_idx = duk_push_array(ctx);
    int count = 0;
    for (ConfigParamMap::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
      {
      duk_push_string(ctx, it->first.c_str());
      duk_put_prop_index(ctx, arr_idx, count++);
      }
    return 1;
    }

  return 0;
  }

static duk_ret_t DukOvmsConfigGetValues(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  std::string prefix = duk_opt_string(ctx,1,"");

  if (!MyConfig.ismounted()) return 0;

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Readable()) return 0;  // Parameter is protected, and not readable

    duk_idx_t obj_idx = duk_push_object(ctx);
    for (ConfigParamMap::iterator it=p->m_map.lower_bound(prefix); it!=p->m_map.end(); ++it)
      {
      if (!startsWith(it->first, prefix)) break;
      duk_push_string(ctx, it->second.c_str());
      duk_put_prop_string(ctx, obj_idx, it->first.substr(prefix.length()).c_str());
      }
    return 1;
    }

  return 0;
  }

static duk_ret_t DukOvmsConfigSetValues(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  std::string prefix = duk_opt_string(ctx,1,"");
  duk_require_object(ctx,2);

  if (!duk_is_object(ctx,2)) return 0;
  if (!MyConfig.ismounted()) return 0;

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Writable()) return 0;  // Parameter is not writeable

    ConfigParamMap pmap = p->m_map;
    std::string key, val;
    duk_enum(ctx, 2, 0);
    while (duk_next(ctx, -1, true))
      {
      key = duk_to_string(ctx, -2);
      val = duk_to_string(ctx, -1);
      duk_pop_2(ctx);
      pmap[prefix+key] = val;
      }
    duk_pop(ctx);
    p->m_map.clear();
    p->m_map = std::move(pmap);
    p->Save();
    }

  return 0;
  }

static duk_ret_t DukOvmsConfigGet(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  const char *instance = duk_to_string(ctx,1);
  const char *defvalue = duk_to_string(ctx,2);

  if (!MyConfig.ismounted()) return 0;
  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Readable()) return 0;  // Parameter is protected, and not readable
    if (p->IsDefined(instance))
      {
      std::string v = p->GetValue(instance);
      duk_push_string(ctx, v.c_str());
      }
    else
      {
      duk_push_string(ctx, defvalue);
      }
    return 1;
    }
  else
    {
    return 0;
    }
  return 0;
  }

static duk_ret_t DukOvmsConfigSet(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  const char *instance = duk_to_string(ctx,1);
  const char *value = duk_to_string(ctx,2);

  if (!MyConfig.ismounted()) return 0;

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Writable()) return 0;  // Parameter is not writeable
    p->SetValue(instance, value);
    }
  return 0;
  }

static duk_ret_t DukOvmsConfigDelete(duk_context *ctx)
  {
  const char *param = duk_to_string(ctx,0);
  const char *instance = duk_to_string(ctx,1);

  if (!MyConfig.ismounted()) return 0;

  OvmsConfigParam *p = MyConfig.CachedParam(param);
  if (p)
    {
    if (! p->Writable()) return 0;  // Parameter is not writeable
    p->DeleteInstance(instance);
    }
  return 0;
  }



/***************************************************************************************************
 * DuktapeObject
 */

/**
 * Construct empty (uncoupled) DuktapeObject
 */
DuktapeObject::DuktapeObject()
  {
  }

/**
 * Construct DuktapeObject coupled to stack object
 *  Stack: unchanged
 */
DuktapeObject::DuktapeObject(duk_context *ctx, int obj_idx)
  {
  Couple(ctx, obj_idx);
  assert(IsCoupled());
  }

/**
 * Destruct DuktapeObject
 *  Note: no JS context here, override Finalize() for JS destructor
 */
DuktapeObject::~DuktapeObject()
  {
  }

/**
 * Ref: increment reference count
 */
void DuktapeObject::Ref()
  {
  Lock();
  m_refcnt++;
  // ESP_LOGD(TAG, "DuktapeObject::Ref cnt=%d", m_refcnt);
  Unlock();
  }

/**
 * Unref: decrement reference count, delete self if no references left
 *  Returns true if instance has been deleted
 */
bool DuktapeObject::Unref()
  {
  Lock();
  assert(m_refcnt > 0);
  m_refcnt--;
  // ESP_LOGD(TAG, "DuktapeObject::Unref cnt=%d", m_refcnt);
  if (m_refcnt == 0)
    {
    delete this;
    return true;
    }
  Unlock();
  return false;
  }

/**
 * Couple: set hidden instance pointer & finalizer on JS object
 *  Stack: unchanged
 */
bool DuktapeObject::Couple(duk_context *ctx, int obj_idx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  duk_require_object(ctx, obj_idx);
  m_object = duk_get_heapptr(ctx, obj_idx);
  if (!m_object) return false;
  duk_require_stack(ctx, 3);
  duk_push_heapptr(ctx, m_object);
  // set instance pointer:
  duk_push_pointer(ctx, this);
  duk_put_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL("duktapeObject"));
  // set finalizer:
  duk_push_c_function(ctx, DuktapeFinalizer, 2 /*(object,heapDestruct)*/);
  duk_set_finalizer(ctx, -2);
  duk_pop(ctx);
  Ref();
  return true;
  }

/**
 * Decouple: clear hidden instance pointer & finalizer on JS object
 *  Stack: unchanged
 */
void DuktapeObject::Decouple(duk_context *ctx)
  {
    {
    OvmsRecMutexLock lock(&m_mutex);
    if (!m_object) return;
    duk_require_stack(ctx, 3);
    duk_push_heapptr(ctx, m_object);
    // clear instance pointer:
    duk_del_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL("duktapeObject"));
    // clear finalizer:
    duk_push_undefined(ctx);
    duk_set_finalizer(ctx, -2);
    duk_pop(ctx);
    m_object = NULL;
    }
  Unref();
  }

/**
 * GetInstance: get hidden instance pointer from JS object
 *  Returns null if no instance is attached
 *  Stack: unchanged
 */
DuktapeObject* DuktapeObject::GetInstance(duk_context *ctx, int obj_idx)
  {
  DuktapeObject* instance = NULL;
  duk_require_stack(ctx, 2);
  if (duk_get_prop_string(ctx, obj_idx, DUK_HIDDEN_SYMBOL("duktapeObject")))
    instance = (DuktapeObject*)duk_get_pointer(ctx, -1);
  duk_pop(ctx);
  return instance;
  }

/**
 * DuktapeFinalizer: Duktape finalizer (destructor) entry
 */
duk_ret_t DuktapeObject::DuktapeFinalizer(duk_context *ctx)
  {
  DuktapeObject* me = GetInstance(ctx, 0);
  bool heapDestruct = duk_opt_boolean(ctx, 1, false);
  if (me) me->Finalize(ctx, heapDestruct);
  return 0;
  }

/**
 * Finalize: JS destructor, called by garbage collection
 */
void DuktapeObject::Finalize(duk_context *ctx, bool heapDestruct)
  {
  // ESP_LOGD(TAG, "DuktapeObject::Finalize heapDestruct=%d", heapDestruct);
  Decouple(ctx); // deletes self if no reference left
  }

/**
 * Push: push coupled JS object onto stack
 *  Stack: [ … ] → [ … obj ]
 */
duk_idx_t DuktapeObject::Push(duk_context *ctx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object) return DUK_INVALID_INDEX;
  duk_require_stack(ctx, 1);
  return duk_push_heapptr(ctx, m_object);
  }

/**
 * Register object with the global stash to prevent garbage collection
 *  Stack: unchanged
 */
void DuktapeObject::Register(duk_context *ctx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object || m_registered) return;
  // ESP_LOGD(TAG, "DuktapeObject::Register @globalStash");
  duk_require_stack(ctx, 3);

  // get registry:
  duk_push_global_stash(ctx);
  if (!duk_get_prop_string(ctx, -1, "duktapeObjectRegistry"))
    {
    // create registry:
    duk_pop(ctx);
    duk_push_object(ctx);
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, -3, "duktapeObjectRegistry");
    }

  // register using instance address as key:
  duk_push_pointer(ctx, this);
  duk_push_heapptr(ctx, m_object);
  duk_put_prop(ctx, -3);

  duk_pop_2(ctx); // registry & global stash

  m_registry = NULL;
  m_registered = true;
  }

/**
 * Register with a specific registry object on the stack
 *  Note: you need to ensure the object is stable until deregistration!
 *  Stack: unchanged
 */
void DuktapeObject::Register(duk_context *ctx, int obj_idx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object || m_registered) return;
  // ESP_LOGD(TAG, "DuktapeObject::Register @object");
  duk_require_object(ctx, obj_idx);
  duk_require_stack(ctx, 3);

  // register using instance address as key:
  duk_dup(ctx, obj_idx);
  duk_push_pointer(ctx, this);
  duk_push_heapptr(ctx, m_object);
  duk_put_prop(ctx, -3);
  duk_pop(ctx);

  m_registry = duk_get_heapptr(ctx, obj_idx);
  m_registered = true;
  }

/**
 * Deregister:
 *  Stack: unchanged
 */
void DuktapeObject::Deregister(duk_context *ctx)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!m_object || !m_registered) return;
  // ESP_LOGD(TAG, "DuktapeObject::Deregister");
  duk_require_stack(ctx, 3);

  if (m_registry)
    {
    duk_push_heapptr(ctx, m_registry);
    duk_push_pointer(ctx, this);
    duk_del_prop(ctx, -2);
    duk_pop(ctx); // registry
    }
  else
    {
    duk_push_global_stash(ctx);
    if (duk_get_prop_string(ctx, -1, "duktapeObjectRegistry"))
      {
      duk_push_pointer(ctx, this);
      duk_del_prop(ctx, -2);
      }
    duk_pop_2(ctx); // registry & global stash
    }

  m_registry = NULL;
  m_registered = false;
  }

/**
 * RequestCallback: request object method call by Duktape context
 */
void DuktapeObject::RequestCallback(const char* method, void* data /*=NULL*/)
  {
  Ref();
  MyScripts.DuktapeRequestCallback(this, method, data);
  }

/**
 * DuktapeCallback: call object method in Duktape context
 */
duk_ret_t DuktapeObject::DuktapeCallback(duk_context *ctx, duktape_queue_t &msg)
  {
  duk_ret_t res = CallMethod(ctx, msg.body.dt_callback.method, msg.body.dt_callback.data);
  Unref();
  return res;
  }

/**
 * CallMethod: execute object method in Duktape context
 *  Default implementation: no args, no return values, no error processing
 */
duk_ret_t DuktapeObject::CallMethod(duk_context *ctx, const char* method, void* data /*=NULL*/)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!IsCoupled()) return 0;
  int entry_top = duk_get_top(ctx);
  Push(ctx);
  duk_push_string(ctx, method);
  duk_call_prop(ctx, -2, 0);
  // discard return values if any + this:
  duk_pop_n(ctx, duk_get_top(ctx) - entry_top);
  return 0;
  }


/***************************************************************************************************
 * DuktapeHTTPRequest
 */

DuktapeHTTPRequest::DuktapeHTTPRequest(duk_context *ctx, int obj_idx)
  : DuktapeObject(ctx, obj_idx)
  {
  if (!MyNetManager.MongooseRunning() || !MyNetManager.m_connected_any)
    {
    m_error = "network unavailable";
    CallMethod(ctx, "fail");
    return;
    }

  // get args:
  duk_require_stack(ctx, 5);
  if (duk_get_prop_string(ctx, 0, "url"))
    m_url = duk_to_string(ctx, -1);
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "post"))
    {
    m_ispost = true;
    m_post = duk_to_string(ctx, -1);
    }
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "timeout"))
    m_timeout = duk_to_int(ctx, -1);
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "binary"))
    m_binary = duk_to_boolean(ctx, -1);
  duk_pop(ctx);

  if (m_url.empty())
    {
    m_error = "missing argument: url";
    CallMethod(ctx, "fail");
    return;
    }

  // …request headers:
  extram::string key, val;
  bool have_useragent = false, have_contenttype = false;
  
  duk_get_prop_string(ctx, 0, "headers");
  if (duk_is_array(ctx, -1))
    {
    for (int i=0; true; i++)
      {
      if (!duk_get_prop_index(ctx, -1, i))
        {
        // array end
        duk_pop(ctx);
        break;
        }
      if (duk_is_object(ctx, -1))
        {
        duk_enum(ctx, -1, 0);
        while (duk_next(ctx, -1, true))
          {
          key = duk_to_string(ctx, -2);
          val = duk_to_string(ctx, -1);
          duk_pop_2(ctx);
          m_headers.append(key);
          m_headers.append(": ");
          m_headers.append(val);
          m_headers.append("\r\n");
          if (key == "User-Agent")
            have_useragent = true;
          else if (key == "Content-Type")
            have_contenttype = true;
          }
        duk_pop(ctx); // enum
        }
      duk_pop(ctx); // array element
      }
    }
  duk_pop(ctx); // [array]
  
  // add defaults:
  if (!have_useragent)
    {
    m_headers.append("User-Agent: ");
    m_headers.append(get_user_agent().c_str());
    m_headers.append("\r\n");
    }
  if (m_ispost && !have_contenttype)
    {
    m_headers.append("Content-Type: application/x-www-form-urlencoded\r\n");
    }

  // start initial request:
  if (StartRequest(ctx))
    {
    // running, prevent deletion & GC:
    Ref();
    Register(ctx);
    ESP_LOGD(TAG, "DuktapeHTTPRequest: started '%s'", m_url.c_str());
    }
  }

bool DuktapeHTTPRequest::StartRequest(duk_context *ctx /*=NULL*/)
  {
  // create connection:
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  struct mg_connect_opts opts = {};
  opts.user_data = this;
  const char* err;
  opts.error_string = &err;
  if (startsWith(m_url, "https://"))
    {
    opts.ssl_ca_cert = "*";
    }
  m_mgconn = mg_connect_http_opt(mgr, MongooseCallbackEntry, opts,
    m_url.c_str(), m_headers.c_str(), m_ispost ? m_post.c_str() : NULL);

  if (!m_mgconn)
    {
    ESP_LOGD(TAG, "DuktapeHTTPRequest: connect to '%s' failed: %s", m_url.c_str(), err);
    m_error = (err && *err) ? err : "unknown";
    if (ctx)
      CallMethod(ctx, "fail");
    else
      RequestCallback("fail");
    return false;
    }

  // connection created:
  if (m_timeout > 0)
    mg_set_timer(m_mgconn, mg_time() + (double)m_timeout / 1000);
  return true;
  }

DuktapeHTTPRequest::~DuktapeHTTPRequest()
  {
  // ESP_LOGD(TAG, "~DuktapeHTTPRequest");
  if (m_mgconn)
    {
    m_mgconn->user_data = NULL;
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    }
  }

duk_ret_t DuktapeHTTPRequest::Create(duk_context *ctx)
  {
  // var request = HTTP.request({ args })
  DuktapeHTTPRequest* request = new DuktapeHTTPRequest(ctx, 0);
  request->Push(ctx);
  return 1;
  }

void DuktapeHTTPRequest::MongooseCallbackEntry(struct mg_connection *nc, int ev, void *ev_data)
  {
  DuktapeHTTPRequest* me = (DuktapeHTTPRequest*)nc->user_data;
  if (me) me->MongooseCallback(nc, ev, ev_data);
  }

void DuktapeHTTPRequest::MongooseCallback(struct mg_connection *nc, int ev, void *ev_data)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (nc != m_mgconn) return; // ignore events of previous connections
  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int err = *(int*) ev_data;
      const char* errdesc = strerror(err);
      ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_CONNECT err=%d/%s", err, errdesc);
      if (err)
        {
        if (err == MG_SSL_ERROR)
          m_error = "SSL error";
        else
          m_error = (errdesc && *errdesc) ? errdesc : "unknown";
        RequestCallback("fail");
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
        }
      }
      break;

    case MG_EV_HTTP_REPLY:
      {
      // response is complete, store:
      http_message *hm = (http_message*) ev_data;
      ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_HTTP_REPLY status=%d bodylen=%d", hm->resp_code, hm->body.len);
      m_response_status = hm->resp_code;
      m_response_statusmsg.assign(hm->resp_status_msg.p, hm->resp_status_msg.len);
      m_response_body.assign(hm->body.p, hm->body.len);
      extram::string key, val, location;
      for (int i = 0; i < MG_MAX_HTTP_HEADERS && hm->header_names[i].len > 0; i++)
        {
        key.assign(hm->header_names[i].p, hm->header_names[i].len);
        val.assign(hm->header_values[i].p, hm->header_values[i].len);
        m_response_headers.push_back(std::make_pair(key, val));
        if (key == "Location") location = val;
        }

      // follow redirect?
      if (m_response_status == 301 || m_response_status == 302)
        {
        if (location.empty())
          {
          m_error = "redirect without location";
          RequestCallback("fail");
          }
        else if (++m_redirectcnt > 5)
          {
          m_error = "too many redirects";
          RequestCallback("fail");
          }
        else
          {
          ESP_LOGD(TAG, "DuktapeHTTPRequest: redirect code=%d to '%s'", m_response_status, location.c_str());
          m_url = location;
          m_response_status = 0;
          m_response_statusmsg.clear();
          m_response_body.clear();
          m_response_headers.clear();
          if (!StartRequest(NULL))
            RequestCallback("fail");
          }
        }
      else
        {
        RequestCallback("done");
        }
      // in any case, this connection is done:
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      }
      break;

    case MG_EV_TIMER:
      {
      ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_TIMER");
      m_error = "timeout";
      RequestCallback("fail");
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      }
      break;

    case MG_EV_CLOSE:
      {
      if (m_response_status == 0 && m_error.empty())
        {
        ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_CLOSE: abort");
        m_error = "abort";
        RequestCallback("fail");
        }
      else
        {
        ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_CLOSE status=%d", m_response_status);
        }
      // Mongoose part done:
      nc->user_data = NULL;
      m_mgconn = NULL;
      Unref();
      }
      break;

    default:
      break;
    }
  }

duk_ret_t DuktapeHTTPRequest::CallMethod(duk_context *ctx, const char* method, void* data /*=NULL*/)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (!IsCoupled()) return 0;

  duk_require_stack(ctx, 7);
  int entry_top = duk_get_top(ctx);

  int obj_idx = Push(ctx);
  duk_get_prop_string(ctx, obj_idx, method);
  bool callable = duk_is_callable(ctx, -1);
  duk_pop(ctx);
  if (callable) duk_push_string(ctx, method);
  int nargs = 0;
  bool deregister = false;

  // update request.url, set request.redirectCount:
  duk_push_string(ctx, m_url.c_str());
  duk_put_prop_string(ctx, obj_idx, "url");
  duk_push_int(ctx, m_redirectcnt);
  duk_put_prop_string(ctx, obj_idx, "redirectCount");

  // create results & method arguments:
  if (strcmp(method, "done") == 0)
    {
    // done(response):
    deregister = true;
    ESP_LOGD(TAG, "DuktapeHTTPRequest: done status=%d bodylen=%d url='%s'",
      m_response_status, m_response_body.size(), m_url.c_str());

    // clear request.error:
    duk_push_string(ctx, "");
    duk_put_prop_string(ctx, obj_idx, "error");

    // create response object:
    duk_push_object(ctx);
    duk_push_int(ctx, m_response_status);
    duk_put_prop_string(ctx, -2, "statusCode");
    duk_push_string(ctx, m_response_statusmsg.c_str());
    duk_put_prop_string(ctx, -2, "statusText");

    // …body:
    if (m_binary)
      {
      void* p = duk_push_fixed_buffer(ctx, m_response_body.size());
      memcpy(p, m_response_body.data(), m_response_body.size());
      duk_put_prop_string(ctx, -2, "data");
      }
    else
      {
      duk_push_lstring(ctx, m_response_body.data(), m_response_body.size());
      duk_put_prop_string(ctx, -2, "body");
      }
    m_response_body.clear();
    m_response_body.shrink_to_fit();

    // …response headers:
    duk_push_array(ctx);
    int i = 0;
    for (auto it = m_response_headers.begin(); it != m_response_headers.end(); it++, i++)
      {
      duk_push_object(ctx);
      duk_push_string(ctx, it->second.c_str());
      duk_put_prop_string(ctx, -2, it->first.c_str());
      duk_put_prop_index(ctx, -2, i);
      }
    duk_put_prop_string(ctx, -2, "headers");
    m_response_headers.clear();

    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, obj_idx, "response");
    nargs++;
    }
  else if (strcmp(method, "fail") == 0)
    {
    // fail(error):
    deregister = true;
    ESP_LOGE(TAG, "DuktapeHTTPRequest: failed error='%s' url='%s'",
      m_error.c_str(), m_url.c_str());

    // set request.error:
    duk_push_string(ctx, m_error.c_str());
    duk_dup(ctx, -1);
    duk_put_prop_string(ctx, obj_idx, "error");
    nargs++;
    }

  // call method:
  if (callable)
    {
    ESP_LOGD(TAG, "DuktapeHTTPRequest: calling method '%s' nargs=%d", method, nargs);
    duk_call_prop(ctx, obj_idx, nargs);
    }

  // clear stack:
  duk_pop_n(ctx, duk_get_top(ctx) - entry_top);
  // allow GC:
  if (deregister) Deregister(ctx);
  return 0;
  }


/***************************************************************************************************
 * OvmsScripts
 */

void OvmsScripts::RegisterDuktapeFunction(duk_c_function func, duk_idx_t nargs, const char* name)
  {
  duktape_registerfunction_t* fn = new duktape_registerfunction_t;
  fn->func = func;
  fn->nargs = nargs;
  m_fnmap[name] = fn;
  }

void OvmsScripts::RegisterDuktapeModule(const char* start, size_t length, const char* name)
  {
  duktape_registermodule_t* mn = new duktape_registermodule_t;
  mn->start = start;
  mn->length = length;
  m_modmap[name] = mn;
  }

duktape_registermodule_t* OvmsScripts::FindDuktapeModule(const char* name)
  {
  auto mod = m_modmap.find(name);
  if (mod == m_modmap.end())
    { return NULL; }
  else
    { return mod->second; }
  }

void OvmsScripts::RegisterDuktapeObject(DuktapeObjectRegistration* ob)
  {
  m_obmap[ob->GetName()] = ob;
  }

void OvmsScripts::AutoInitDuktape()
  {
  if (MyConfig.GetParamValueBool("auto", "scripting", true))
    {
    duktape_queue_t dmsg;
    memset(&dmsg, 0, sizeof(dmsg));
    dmsg.type = DUKTAPE_autoinit;
    DuktapeDispatch(&dmsg);
    }
  }

void OvmsScripts::DuktapeDispatch(duktape_queue_t* msg)
  {
  msg->waitcompletion = NULL;
  xQueueSend(m_duktaskqueue, msg, portMAX_DELAY);
  }

void OvmsScripts::DuktapeDispatchWait(duktape_queue_t* msg)
  {
  msg->waitcompletion = xSemaphoreCreateBinary();
  xQueueSend(m_duktaskqueue, msg, portMAX_DELAY);
  xSemaphoreTake(msg->waitcompletion, portMAX_DELAY);
  vSemaphoreDelete(msg->waitcompletion);
  msg->waitcompletion = NULL;
  }

void OvmsScripts::DuktapeEvalNoResult(const char* text, OvmsWriter* writer)
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_evalnoresult;
  dmsg.writer = writer;
  dmsg.body.dt_evalnoresult.text = text;
  DuktapeDispatchWait(&dmsg);
  }

float OvmsScripts::DuktapeEvalFloatResult(const char* text, OvmsWriter* writer)
  {
  float result = 0;
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_evalfloatresult;
  dmsg.writer = writer;
  dmsg.body.dt_evalfloatresult.text = text;
  dmsg.body.dt_evalfloatresult.result = &result;
  DuktapeDispatchWait(&dmsg);
  return result;
  }

int OvmsScripts::DuktapeEvalIntResult(const char* text, OvmsWriter* writer)
  {
  int result = 0;
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_evalintresult;
  dmsg.writer = writer;
  dmsg.body.dt_evalintresult.text = text;
  dmsg.body.dt_evalintresult.result = &result;
  DuktapeDispatchWait(&dmsg);
  return result;
  }

void OvmsScripts::DuktapeReload()
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_reload;
  DuktapeDispatchWait(&dmsg);
  }

void OvmsScripts::DuktapeCompact()
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_compact;
  DuktapeDispatchWait(&dmsg);
  }

void OvmsScripts::DuktapeRequestCallback(DuktapeObject* instance, const char* method, void* data)
  {
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_callback;
  dmsg.body.dt_callback.instance = instance;
  dmsg.body.dt_callback.method = method;
  dmsg.body.dt_callback.data = data;
  DuktapeDispatch(&dmsg);
  }

void *DukAlloc(void *udata, duk_size_t size)
  {
  return NULL;
  }

void OvmsScripts::DukTapeInit()
  {
  ESP_LOGI(TAG,"Duktape: Creating heap");
  m_dukctx = duk_create_heap(DukOvmsAlloc,
    DukOvmsRealloc,
    DukOvmsFree,
    this,
    DukOvmsFatalHandler);

  ESP_LOGI(TAG,"Duktape: Initialising module system");
  duk_push_object(m_dukctx);
  duk_push_c_function(m_dukctx, DukOvmsResolveModule, DUK_VARARGS);
  duk_put_prop_string(m_dukctx, -2, "resolve");
  duk_push_c_function(m_dukctx, DukOvmsLoadModule, DUK_VARARGS);
  duk_put_prop_string(m_dukctx, -2, "load");
  duk_module_node_init(m_dukctx);

  if (m_fnmap.size() > 0)
    {
    // We have some functions to register...
    DuktapeFunctionMap::iterator itm=m_fnmap.begin();
    while (itm!=m_fnmap.end())
      {
      const char* name = itm->first;
      duktape_registerfunction_t* fn = itm->second;
      ESP_LOGD(TAG,"Duktape: Pre-Registered function %s",name);
      duk_push_c_function(m_dukctx, fn->func, fn->nargs);
      duk_put_global_string(m_dukctx, name);
      ++itm;
      }
    }

  if (m_obmap.size() > 0)
    {
    // We have some objects to register...
    DuktapeObjectMap::iterator itm=m_obmap.begin();
    while (itm!=m_obmap.end())
      {
      const char* name = itm->first;
      DuktapeObjectRegistration* ob = itm->second;
      ESP_LOGD(TAG,"Duktape: Pre-Registered object %s",name);
      ob->RegisterWithDuktape(m_dukctx);
      ++itm;
      }
    }

  // Load registered modules
  if (m_modmap.size() > 0)
    {
    // We have some modules to register...
    DuktapeModuleMap::iterator itm=m_modmap.begin();
    while (itm!=m_modmap.end())
      {
      const char* name = itm->first;
      ESP_LOGD(TAG,"Duktape: Pre-Registered module %s",name);
      std::string loadfn("(function(){");
      loadfn.append(name);
      loadfn.append("=require(\"int/");
      loadfn.append(name);
      loadfn.append("\");})();");
      duk_push_string(m_dukctx, loadfn.c_str());
      if (duk_peval(m_dukctx) != 0)
        {
        ESP_LOGE(TAG,"Duktape: %s",duk_safe_to_string(m_dukctx, -1));
        }
      duk_pop(m_dukctx);
      ++itm;
      }
    }

  // ovmsmain
  FILE* sf = fopen("/store/scripts/ovmsmain.js", "r");
  if (sf != NULL)
    {
    fseek(sf,0,SEEK_END);
    long slen = ftell(sf);
    fseek(sf,0,SEEK_SET);
    char *script = new char[slen+1];
    memset(script,0,slen+1);
    fread(script,1,slen,sf);
    duk_push_string(m_dukctx, script);
    delete [] script;
    fclose(sf);
    ESP_LOGI(TAG,"Duktape: Executing ovmsmain.js");
    duk_module_node_peval_main(m_dukctx, "ovmsmain.js");
    }
  }

void OvmsScripts::DukTapeTask()
  {
  duktape_queue_t msg;

  ESP_LOGI(TAG,"Duktape: Scripting task is running");
  esp_task_wdt_add(NULL); // WATCHDOG is active for this task

  while(1)
    {
    if (xQueueReceive(m_duktaskqueue, &msg, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      esp_task_wdt_reset(); // Reset WATCHDOG timer for this task
      duktapewriter = msg.writer;
      switch(msg.type)
        {
        case DUKTAPE_reload:
          {
          // Reload DUKTAPE engine
          if (m_dukctx != NULL)
            {
            ESP_LOGI(TAG,"Duktape: Clearing existing context");
            duk_destroy_heap(m_dukctx);
            m_dukctx = NULL;
            }
          DukTapeInit();
          }
          break;
        case DUKTAPE_compact:
          {
          // Compact DUKTAPE memory
          if (m_dukctx != NULL)
            {
            ESP_LOGI(TAG,"Duktape: Compacting DukTape memory");
            duk_gc(m_dukctx, 0);
            duk_gc(m_dukctx, 0);
            }
          }
          break;
        case DUKTAPE_event:
          {
          // Event
          if (m_dukctx != NULL)
            {
            // Deliver the event to DUKTAPE
            duk_get_global_string(m_dukctx, "PubSub");
            duk_get_prop_string(m_dukctx, -1, "publish");
            duk_dup(m_dukctx, -2);  /* this binding = process */
            duk_push_string(m_dukctx, msg.body.dt_event.name);
            duk_push_string(m_dukctx, "");
            if (duk_pcall_method(m_dukctx, 2) != 0)
              {
              ESP_LOGE(TAG,"Duktape: %s",duk_safe_to_string(m_dukctx, -1));
              }
            duk_pop_2(m_dukctx);
            }
          }
          break;
        case DUKTAPE_autoinit:
          {
          // Auto init
          DukTapeInit();
          }
          break;
        case DUKTAPE_evalnoresult:
          if (m_dukctx != NULL)
            {
            // Execute script text (without result)
            duk_push_string(m_dukctx, msg.body.dt_evalnoresult.text);
            if (duk_peval(m_dukctx) != 0)
              {
              ESP_LOGE(TAG,"Duktape: %s",duk_safe_to_string(m_dukctx, -1));
              }
            duk_pop(m_dukctx);
            }
          break;
        case DUKTAPE_evalfloatresult:
          if (m_dukctx != NULL)
            {
            // Execute script text (float result)
            duk_push_string(m_dukctx, msg.body.dt_evalfloatresult.text);
            if (duk_peval(m_dukctx) != 0)
              {
              ESP_LOGE(TAG,"Duktape: %s",duk_safe_to_string(m_dukctx, -1));
              *msg.body.dt_evalfloatresult.result = 0;
              }
            else
              {
              *msg.body.dt_evalfloatresult.result = (float)duk_get_number(m_dukctx,-1);
              }
            duk_pop(m_dukctx);
            }
          break;
        case DUKTAPE_evalintresult:
          if (m_dukctx != NULL)
            {
            // Execute script text (int result)
            duk_push_string(m_dukctx, msg.body.dt_evalintresult.text);
            if (duk_peval(m_dukctx) != 0)
              {
              ESP_LOGE(TAG,"Duktape: %s",duk_safe_to_string(m_dukctx, -1));
              *msg.body.dt_evalintresult.result = 0;
              }
            else
              {
              *msg.body.dt_evalintresult.result = (int)duk_get_int(m_dukctx,-1);
              }
            duk_pop(m_dukctx);
            }
          break;
        case DUKTAPE_callback:
          if (m_dukctx != NULL)
            {
            // DuktapeObject callback (without result)
            DuktapeObject* dto = msg.body.dt_callback.instance;
            dto->DuktapeCallback(m_dukctx, msg);
            }
          break;
        default:
          ESP_LOGE(TAG,"Duktape: Unrecognised msg type 0x%04x",msg.type);
          break;
        }
      duktapewriter = NULL;
      if (msg.waitcompletion)
        {
        // Signal the completion...
        xSemaphoreGive(msg.waitcompletion);
        }
      }
    esp_task_wdt_reset(); // Reset WATCHDOG timer for this task
    }
  }

static void script_reload(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Reloading javascript engine");
  MyScripts.DuktapeReload();
  }

static void script_eval(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyScripts.DuktapeEvalNoResult(argv[0], writer);
  }

static void script_compact(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Compacting javascript memory");
  MyScripts.DuktapeCompact();
  }

#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

static void script_ovms(bool print, int verbosity, OvmsWriter* writer,
  const char* spath, FILE* sf, bool secure=false)
  {
  char *ext = rindex(spath, '.');
  if ((ext != NULL)&&(strcmp(ext,".js")==0))
    {
    // Javascript script
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    fseek(sf,0,SEEK_END);
    long slen = ftell(sf);
    fseek(sf,0,SEEK_SET);
    char *script = new char[slen+1];
    memset(script,0,slen+1);
    fread(script,1,slen,sf);
    MyScripts.DuktapeEvalNoResult(script, writer);
    delete [] script;
#else // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    if (writer)
      {
      writer->puts("Error: No javascript engine available");
      }
    else
      {
      ESP_LOGE(TAG, "Error: No javascript engine available");
      }
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    fclose(sf);
    }
  else
    {
    // Default: OVMS command script
    BufferedShell* bs = new BufferedShell(print, verbosity);
    if (secure) bs->SetSecure(true);
    char* cmdline = new char[_COMMAND_LINE_LEN];
    while(fgets(cmdline, _COMMAND_LINE_LEN, sf) != NULL )
      {
      bs->ProcessChars(cmdline, strlen(cmdline));
      }
    fclose(sf);
    if (writer)
      {
      bs->Output(writer);
      }
    else
      {
      extram::string output;
      bs->Dump(output);
      ESP_LOGI(TAG, "%s", output.c_str());
      }
    delete bs;
    delete [] cmdline;
    }
  }

static void script_run(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  FILE *sf = NULL;
  std::string path;

  if (argv[0][0] == '/')
    {
    // A direct path specification
    path = std::string(argv[0]);
    sf = fopen(argv[0], "r");
    }
  else
    {
#ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS
    path = std::string("/sd/scripts/");
    path.append(argv[0]);
    sf = fopen(path.c_str(), "r");
#endif // #ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS
    if (sf == NULL)
      {
      path = std::string("/store/scripts/");
      path.append(argv[0]);
      sf = fopen(path.c_str(), "r");
      }
    }
  if (sf == NULL)
    {
    writer->puts("Error: Script not found");
    return;
    }
  script_ovms(verbosity != COMMAND_RESULT_MINIMAL, verbosity, writer,
    path.c_str(), sf, writer->IsSecure());
  }

void OvmsScripts::AllScripts(std::string path)
  {
  DIR *dir;
  struct dirent *dp;
  FILE *sf;
  std::set<std::string> files;

  // read dir, sort scripts by name:
  if ((dir = opendir (path.c_str())) != NULL)
    {
    while ((dp = readdir (dir)) != NULL)
      {
      std::string fpath = path;
      fpath.append("/");
      fpath.append(dp->d_name);
      files.insert(fpath);
      }
    closedir(dir);
    }

  // execute scripts:
  for (auto it = files.begin(); it != files.end(); it++)
    {
    std::string fpath = *it;
    sf = fopen(fpath.c_str(), "r");
    if (sf)
      {
      ESP_LOGI(TAG, "Running script %s", fpath.c_str());
      script_ovms(false, COMMAND_RESULT_MINIMAL, NULL, fpath.c_str(), sf, true);
      // script_ovms() closes sf
      }
    }
  }

void OvmsScripts::EventScript(std::string event, void* data)
  {
  std::string path;

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  // dispatch event to PubSub component:
  duktape_queue_t dmsg;
  memset(&dmsg, 0, sizeof(dmsg));
  dmsg.type = DUKTAPE_event;
  dmsg.body.dt_event.name = event.c_str();
  dmsg.body.dt_event.data = data;
  DuktapeDispatchWait(&dmsg);
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

#ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS
  // run event scripts on external storage:
  path=std::string("/sd/events/");
  path.append(event);
  AllScripts(path);
#endif // #ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS

  // run event scripts on internal storage:
  path=std::string("/store/events/");
  path.append(event);
  AllScripts(path);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  if (event == "ticker.60")
    {
    // do garbage collection once per minute:
    DuktapeCompact();
    }
#endif // CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

OvmsScripts::OvmsScripts()
  {
  ESP_LOGI(TAG, "Initialising SCRIPTS (1600)");

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE
  ESP_LOGI(TAG, "No javascript engines enabled (command scripting only)");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  ESP_LOGI(TAG, "Using DUKTAPE javascript engine");

  // Register standard modules...
  extern const char mod_pubsub_js_start[]     asm("_binary_pubsub_js_start");
  extern const char mod_pubsub_js_end[]       asm("_binary_pubsub_js_end");
  RegisterDuktapeModule(mod_pubsub_js_start, mod_pubsub_js_end - mod_pubsub_js_start, "PubSub");

  extern const char mod_json_js_start[]     asm("_binary_json_js_start");
  extern const char mod_json_js_end[]       asm("_binary_json_js_end");
  RegisterDuktapeModule(mod_json_js_start, mod_json_js_end - mod_json_js_start, "JSON");

  // Register standard functions
  RegisterDuktapeFunction(DukOvmsPrint, 1, "print");
  RegisterDuktapeFunction(DukOvmsAssert, 2, "assert");
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsCommand");
  dto->RegisterDuktapeFunction(DukOvmsCommand, 1, "Exec");
  RegisterDuktapeObject(dto);
  dto = new DuktapeObjectRegistration("OvmsEvents");
  dto->RegisterDuktapeFunction(DukOvmsRaiseEvent, 2, "Raise");
  RegisterDuktapeObject(dto);
  dto = new DuktapeObjectRegistration("OvmsConfig");
  dto->RegisterDuktapeFunction(DukOvmsConfigParams, 0, "Params");
  dto->RegisterDuktapeFunction(DukOvmsConfigInstances, 1, "Instances");
  dto->RegisterDuktapeFunction(DukOvmsConfigGet, 3, "Get");
  dto->RegisterDuktapeFunction(DukOvmsConfigSet, 3, "Set");
  dto->RegisterDuktapeFunction(DukOvmsConfigDelete, 2, "Delete");
  dto->RegisterDuktapeFunction(DukOvmsConfigGetValues, 2, "GetValues");
  dto->RegisterDuktapeFunction(DukOvmsConfigSetValues, 3, "SetValues");
  MyScripts.RegisterDuktapeObject(dto);
  DuktapeObjectRegistration* dt_http = new DuktapeObjectRegistration("HTTP");
  dt_http->RegisterDuktapeFunction(DuktapeHTTPRequest::Create, 1, "request");
  RegisterDuktapeObject(dt_http);

  // Start the DukTape task...
  m_duktaskqueue = xQueueCreate(CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_QUEUE_SIZE,sizeof(duktape_queue_t));
  xTaskCreatePinnedToCore(DukTapeLaunchTask, "OVMS DukTape",
                          CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_STACK, (void*)this,
                          CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE_PRIORITY, &m_duktaskid, CORE(1));
  AddTaskToMap(m_duktaskid);
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

  OvmsCommand* cmd_script = MyCommandApp.RegisterCommand("script","SCRIPT framework");
  cmd_script->RegisterCommand("run","Run a script",script_run,"<path>",1,1);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  cmd_script->RegisterCommand("reload","Reload javascript framework",script_reload);
  cmd_script->RegisterCommand("eval","Eval some javascript code",script_eval,"<code>",1,1);
  cmd_script->RegisterCommand("compact","Compact javascript heap",script_compact);
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  MyCommandApp.RegisterCommand(".","Run a script",script_run,"<path>",1,1);
  }

OvmsScripts::~OvmsScripts()
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  duk_destroy_heap(m_dukctx);
  m_dukctx = NULL;
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }
