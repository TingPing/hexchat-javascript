/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _WIN32
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif

#include <string>
#include <fstream>
#include <list>

#ifdef _WIN32
#include <Shlwapi.h> // For PathIsRelative
#include "win32/dirent-win32.h"
#include "win32/hexchat-plugin.h"
#else
#include <dirent.h>
#include <hexchat-plugin.h>
#endif
#include <jsapi.h>


#define HJS_VERSION_STR "0.3"
#define HJS_VERSION_FLOAT 0.3

#define JSSTRING_TO_CHAR(jsstr) JS_EncodeString(context, jsstr)
#define DEFINE_GLOBAL_PROP(name, value) JS_DefineProperty (*cx, *globals, name, value, nullptr, nullptr, \
														JSPROP_READONLY|JSPROP_PERMANENT)

#ifdef _WIN32
#define DIR_SEP '\\'
#else
#define DIR_SEP '/'
#endif

using namespace std;

static hexchat_plugin *ph;
static char* name = "javascript";
static char* version = HJS_VERSION_STR;
static char* description = "Javascript scripting interface";
static const char* help = "Usage: JS <command>\n       Use LOAD, UNLOAD, RELOAD or Window > Plugins… to manage scripts.";

static JSRuntime *interp_rt;
static JSContext *interp_cx;
static JSObject  *interp_globals;

static JSClass global_class = {"global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS};
static JSClass list_entry_class = {"list_entry", JSPROP_READONLY|JSPROP_ENUMERATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS};

enum hook_type
{
	HOOK_CMD,
	HOOK_PRINT,
	HOOK_SERVER,
	HOOK_TIMER,
	HOOK_UNLOAD
};

typedef struct
{
	JSContext* context;
	JSObject* callback;
	JSObject* userdata;
	hexchat_hook* hook;
	hook_type type;
} script_hook;

class js_script
{
	private:
		JSRuntime* runtime;
		JSObject* globals;
		string desc;
		string version;

	public:
		JSContext* context;
		string name;
		void* gui;
		string filename;
		list<script_hook*> hooks;

		js_script (string, string);
		void add_hook (script_hook*, hook_type, JSContext*, JSObject*, JSObject*, hexchat_hook*);
		void remove_hook (script_hook*);
		~js_script ();
};

static list<js_script*> js_script_list;


/* utility functions */

static string
hjs_util_expandfile (string file)
{
	string expanded = file;
	bool absolute = false;

#ifdef _WIN32
	if (!PathIsRelative(file.c_str()))
		absolute = true;
#else
	if (file[0] == '/')
		absolute = true;
#endif

	if (!absolute)
	{
#ifndef _WIN32
		if (file[0] == '~')
			expanded = string(getenv("HOME")) + DIR_SEP + file.substr(2);
		else
#endif
			expanded = string(hexchat_get_info (ph, "configdir")) + DIR_SEP + "addons" + DIR_SEP + file;
	}

	return expanded;
}

static string
hjs_util_shrinkfile (string file)
{
	return file.substr (file.rfind(DIR_SEP) + 1);
}

// http://insanecoding.blogspot.com/2011/11/how-to-read-in-file-in-c.html
static string
hjs_util_getcontents (string filename)
{
	ifstream in(filename, ios::in | ios::binary);
	if (in.good())
	{
		string contents;
		in.seekg(0, ios::end);
		contents.resize(in.tellg());
		in.seekg(0, ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	return "";
}

static bool
hjs_util_isscript (string file)
{
	if (file.find(".js", file.length() - 3) != string::npos)
		return true;

	return false;
}

static jsval
hjs_util_buildword (JSContext* context, char* word[])
{
	JSObject* wordlist = JS_NewArrayObject (context, 0, nullptr);

	for (int i = 0; word[i][0]; i++)
	{
		JSString* str = JS_NewStringCopyZ (context, word[i]);
		JS_DefineElement (context, wordlist, i, STRING_TO_JSVAL(str), nullptr, nullptr,
						JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_ENUMERATE);
	}

	return OBJECT_TO_JSVAL(wordlist);
}

// from gjs/jsapi-util.c
static jsval
hjs_util_datefromtime (JSContext *context, time_t server_time)
{
	JSObject *date;
	JSClass *date_class;
	JSObject *date_constructor;
	jsval date_prototype;
	jsval args[1];
	struct tm *timeinfo;

	// Use current time if no time is given
	if (server_time == 0)
	{
		server_time = time(0);
		timeinfo = localtime(&server_time);
		server_time = mktime(timeinfo);
	}

	if (!JS_EnterLocalRootScope(context))
		return JSVAL_VOID;

	if (!JS_GetClassObject(context, JS_GetGlobalObject(context), JSProto_Date, &date_constructor))
		return JSVAL_VOID;

	if (!JS_GetProperty(context, date_constructor, "prototype", &date_prototype))
		return JSVAL_VOID;

	date_class = JS_GetClass(context, JSVAL_TO_OBJECT (date_prototype));

	if (!JS_NewNumberValue(context, ((double) server_time) * 1000, &(args[0])))
		return JSVAL_VOID;

	date = JS_ConstructObjectWithArguments(context, date_class, nullptr, nullptr, 1, args);

	JS_LeaveLocalRootScope(context);
	return OBJECT_TO_JSVAL(date);
}

static time_t
hjs_util_timefromdate (JSContext *context, JSObject *date)
{
	jsval retval;

	if (!JS_CallFunctionName(context, date, "getTime", 0, nullptr, &retval))
		return (time_t)0;

	if (JSVAL_IS_INT(retval))
		return (time_t)(JSVAL_TO_INT(retval));

	return (time_t)0;
}


/* script functions */

static js_script*
hjs_script_find (string name)
{
	// searches by filename OR scriptname
	for (js_script* script : js_script_list)
		if (script->filename == name
			|| hjs_util_shrinkfile(script->filename) == name
			|| script->name == name)
			return script;

	return nullptr;
}

static js_script*
hjs_script_find (JSContext* context)
{
	for (js_script* script : js_script_list)
		if (script->context == context)
			return script;

	return nullptr;
}

static hexchat_plugin*
hjs_script_gethandle (JSContext* context)
{
	js_script* script = hjs_script_find (context);

	if (script != nullptr && script->gui != nullptr)
		return (hexchat_plugin*)script->gui;
	else
		return ph;
}

static string
hjs_script_getproperty (JSContext* context, string property, string fallback)
{
	JSObject* globals = JS_GetGlobalForScopeChain (context);
	JSBool found;
	jsval retval;
	char* cstr;
	string str;

	if (JS_HasProperty (context, globals, property.c_str(), &found))
	{
		if (found)
		{
			JS_GetProperty (context, globals, property.c_str(), &retval);
			cstr = JSSTRING_TO_CHAR(JSVAL_TO_STRING(retval));
			str = string(cstr);
			JS_free(context, cstr);
			return str;
		}
	}

	return fallback;
}

static void
hjs_script_cleanup ()
{
	for (js_script* script : js_script_list)
		delete script;
}

static bool
hjs_script_load (string file)
{
	string src;

	if (hjs_util_isscript (file))
	{
		file = hjs_util_expandfile (file);
		src = hjs_util_getcontents (file);

		if (!src.empty())
			new js_script (file, src);

		return true;
	}

	return false;
}

static bool
hjs_script_unload (string file)
{
	js_script* script;

	script = hjs_script_find (file);
	if (script)
	{
		js_script_list.remove (script);
		delete script;
		return true;
	}

	return false;
}

static bool
hjs_script_reload (string file)
{
	string oldfile;
	js_script* script;

	script = hjs_script_find (file);
	if (script)
	{
		oldfile = script->filename;
		hjs_script_unload (oldfile);
		hjs_script_load (oldfile);
		return true;
	}

	return false;
}

static void
hjs_script_autoload ()
{
	DIR* dir;
	dirent* ent;
	string file;
	string path = hjs_util_expandfile ("");

	dir = opendir (path.c_str());
	while ((ent = readdir (dir)))
	{
		file = string(ent->d_name);
		if (hjs_util_isscript (file))
			hjs_script_load (file);
	}
	closedir (dir);
}


/* hexchat commands */

static int
hjs_load_cb (char* word[], char* word_eol[], void *userdata)
{
	if (hjs_script_load (string(word[2])))
		return HEXCHAT_EAT_ALL;
	else
		return HEXCHAT_EAT_NONE;
}

static int
hjs_unload_cb (char* word[], char* word_eol[], void *userdata)
{
	if (hjs_script_unload (string(word[2])))
		return HEXCHAT_EAT_ALL;
	else
		return HEXCHAT_EAT_NONE;
}

static int
hjs_reload_cb (char* word[], char* word_eol[], void *userdata)
{
	if (hjs_script_reload (string(word[2])))
		return HEXCHAT_EAT_ALL;
	else
		return HEXCHAT_EAT_NONE;
}

static int
hjs_cmd_cb (char* word[], char* word_eol[], void *userdata)
{
	jsval rval = JSVAL_VOID;
	JSString* str;
	char* ret;

	if (word[2][0] != 0)
	{
		if (JS_EvaluateScript (interp_cx, interp_globals, word_eol[2], strlen (word_eol[2]), "", 0, &rval))
		{
			str = JS_ValueToString (interp_cx, rval);
			if (!JSVAL_IS_VOID(rval) && str != nullptr)
			{
				ret = JS_EncodeString (interp_cx, str);
				// fancy blue message, might be annoying but otherwise confusing
				hexchat_printf (ph, "\00318JavaScript Output:\017 %s", ret);
				JS_free(interp_cx, ret);
			}
		}
	}
	else
		hexchat_command (ph, "help js");

	return HEXCHAT_EAT_ALL;
}

static void
hjs_print_error (JSContext* context, const char* message, JSErrorReport* report)
{
	js_script* script = hjs_script_find (context);
	string file;
	if (script)
		file = hjs_util_shrinkfile (script->filename);

	if (!file.empty())
	{
		hexchat_printf (ph, "\00320JavaScript Error in \"%s\":\017 %s", file.c_str(), message);
		hjs_script_unload (file); // It stops executing the script an error, unload it
	}
	else
		hexchat_printf (ph, "\00320JavaScript Error:\017 %s", message);
}


/* callback functions for hooks */

static int
hjs_callback (char* word[], char* word_eol[], hexchat_event_attrs *attrs, void *hook) // server
{
	JSContext* context = ((script_hook*)hook)->context;
	JSFunction* fun = JS_ValueToFunction (context, OBJECT_TO_JSVAL(((script_hook*)hook)->callback));
	jsval argv[4];
	jsval rval = JSVAL_VOID;

	argv[0] = hjs_util_buildword (context, word+1);
	argv[1] = hjs_util_buildword (context, word_eol+1);
	argv[2] = hjs_util_datefromtime (context, attrs->server_time_utc);
	argv[3] = OBJECT_TO_JSVAL(((script_hook*)hook)->userdata);

	JS_CallFunction (context, JS_GetGlobalForScopeChain (context), fun, 4, argv, &rval);

	if (JSVAL_IS_VOID(rval))
		return HEXCHAT_EAT_NONE;
	else
		return JSVAL_TO_INT(rval);
}

static int
hjs_callback (char* word[], char* word_eol[], void *hook) // command
{
	JSContext* context = ((script_hook*)hook)->context;
	JSFunction* fun = JS_ValueToFunction (context, OBJECT_TO_JSVAL(((script_hook*)hook)->callback));
	jsval argv[3];
	jsval rval = JSVAL_VOID;

	argv[0] = hjs_util_buildword (context, word+1);
	argv[1] = hjs_util_buildword (context, word_eol+1);
	argv[2] = OBJECT_TO_JSVAL(((script_hook*)hook)->userdata);

	JS_CallFunction (context, JS_GetGlobalForScopeChain (context), fun, 3, argv, &rval);

	if (JSVAL_IS_VOID(rval))
		return HEXCHAT_EAT_NONE;
	else
		return JSVAL_TO_INT(rval);
}

static int
hjs_callback (char* word[], hexchat_event_attrs *attrs, void *hook) // print
{
	JSContext* context = ((script_hook*)hook)->context;
	JSFunction* fun = JS_ValueToFunction (context, OBJECT_TO_JSVAL(((script_hook*)hook)->callback));
	jsval argv[3];
	jsval rval = JSVAL_VOID;

	argv[0] = hjs_util_buildword (context, word+1);
	argv[1] = hjs_util_datefromtime (context, attrs->server_time_utc);
	argv[2] = OBJECT_TO_JSVAL(((script_hook*)hook)->userdata);

	JS_CallFunction (context, JS_GetGlobalForScopeChain (context), fun, 3, argv, &rval);

	if (JSVAL_IS_VOID(rval))
		return HEXCHAT_EAT_NONE;
	else
		return JSVAL_TO_INT(rval);
}

static int
hjs_callback (char* word[], void *hook) // special
{
	JSContext* context = ((script_hook*)hook)->context;
	JSFunction* fun = JS_ValueToFunction (context, OBJECT_TO_JSVAL(((script_hook*)hook)->callback));
	jsval argv[2];
	jsval rval = JSVAL_VOID;

	argv[0] = hjs_util_buildword (context, word+1);
	argv[1] = OBJECT_TO_JSVAL(((script_hook*)hook)->userdata);

	JS_CallFunction (context, JS_GetGlobalForScopeChain (context), fun, 2, argv, &rval);

	if (JSVAL_IS_VOID(rval))
		return HEXCHAT_EAT_NONE;
	else
		return JSVAL_TO_INT(rval);
}

static int
hjs_callback (void *hook) // timer
{
	JSContext* context = ((script_hook*)hook)->context;
	JSFunction* fun = JS_ValueToFunction (context, OBJECT_TO_JSVAL(((script_hook*)hook)->callback));
	jsval argv[1];
	jsval rval = JSVAL_VOID;

	argv[0] = OBJECT_TO_JSVAL(((script_hook*)hook)->userdata);

	JS_CallFunction (context, JS_GetGlobalForScopeChain (context), fun, 1, argv, &rval);

	if (JSVAL_IS_VOID(rval))
		return HEXCHAT_EAT_NONE;
	else
		return JSVAL_TO_INT(rval);
}


/* js functions */

static JSBool
hjs_print (JSContext *context, unsigned argc, jsval *vp)
{
	JSObject* obj;
	JSString* str;
	char* cstr;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "o", &obj))
		return JS_FALSE;

	str = JS_ValueToString (context, OBJECT_TO_JSVAL(obj));
	if (str != nullptr)
	{
		cstr = JSSTRING_TO_CHAR(str);
		hexchat_print (ph, cstr);
		JS_free(context, cstr);
	}

	JS_SET_RVAL (context, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool
hjs_emitprint (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* name;
	JSString* args[5] = { nullptr };
	char* carg[5] = { nullptr };
	char* cname;
	int ret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S/SSSSS",
							&name, &args[0], &args[1], &args[2], &args[3], &args[4]))
		return JS_FALSE;

	// convert all jsstrings
	for (int i = 0; args[i]; i++)
		carg[i] = JSSTRING_TO_CHAR(args[i]);

	cname = JSSTRING_TO_CHAR(name);

	ret = hexchat_emit_print (ph, cname, carg[0], carg[1],
								carg[2], carg[3], carg[4], nullptr);

	JS_free(context, cname);

	for (int i = 0; carg[i]; i++)
		JS_free(context, carg[i]);

	JS_SET_RVAL (context, vp, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
hjs_emitprintat (JSContext *context, unsigned argc, jsval *vp)
{
	JSObject* date;
	JSString* name;
	JSString* args[5] = { nullptr };
	char* carg[5] = { nullptr };
	char* cname;
	hexchat_event_attrs* attrs;
	int ret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "oS/SSSSS",
							&date, &name, &args[0], &args[1], &args[2], &args[3], &args[4]))
		return JS_FALSE;

	// convert all jsstrings
	for (int i = 0; args[i]; i++)
		carg[i] = JSSTRING_TO_CHAR(args[i]);

	cname = JSSTRING_TO_CHAR(name);

	attrs = hexchat_event_attrs_create(ph);
	attrs->server_time_utc = hjs_util_timefromdate(context, date);

	ret = hexchat_emit_print_attrs (ph, attrs, cname, carg[0], carg[1],
								carg[2], carg[3], carg[4], nullptr);

	hexchat_event_attrs_free(ph, attrs);
	JS_free(context, cname);

	for (int i = 0; carg[i]; i++)
		JS_free(context, carg[i]);

	JS_SET_RVAL (context, vp, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
hjs_command (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* cmd;
	char* ccmd;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S", &cmd))
		return JS_FALSE;

	ccmd = JSSTRING_TO_CHAR(cmd);
	hexchat_command (ph, ccmd);
	JS_free(context, ccmd);

	JS_SET_RVAL (context, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool
hjs_nickcmp (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* nick1;
	JSString* nick2;
	char* cnick1;
	char* cnick2;
	int ret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "SS", &nick1, &nick2))
		return JS_FALSE;

	cnick1 = JSSTRING_TO_CHAR(nick1);
	cnick2 = JSSTRING_TO_CHAR(nick2);
	ret = hexchat_nickcmp (ph, cnick1, cnick2);
	JS_free(context, cnick1);
	JS_free(context, cnick2);

	JS_SET_RVAL (context, vp, INT_TO_JSVAL (ret));

	return JS_TRUE;
}

static JSBool
hjs_strip (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* str;
	JSString* ret;
	char* cstr;
	char* cret;
	int flags = 3;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S/i", &str, &flags))
		return JS_FALSE;

	cstr = JSSTRING_TO_CHAR(str);
	cret = hexchat_strip (ph, cstr, -1, flags);
	JS_free(context, cstr);

	if (cret == nullptr)
	{
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	}
	else
	{
		ret = JS_NewStringCopyZ (context, cret);
		JS_SET_RVAL (context, vp, STRING_TO_JSVAL(ret));

		hexchat_free (ph, cret);
	}

	return JS_TRUE;
}

static JSBool
hjs_getinfo (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* str;
	JSString* ret;
	char* cstr;
	const char* cret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S", &str))
		return JS_FALSE;

	cstr = JSSTRING_TO_CHAR(str);
	cret = hexchat_get_info (ph, cstr);
	JS_free(context, cstr);

	if (cret == nullptr)
	{
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	}
	else
	{
		ret = JS_NewStringCopyZ (context, cret);
		JS_SET_RVAL (context, vp, STRING_TO_JSVAL(ret));
	}

	return JS_TRUE;
}

static JSBool
hjs_getprefs (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* str;
	JSString* ret;
	char* cstr;
	const char* cstrret;
	int intret, cret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S", &str))
		return JS_FALSE;

	cstr = JSSTRING_TO_CHAR(str);
	cret = hexchat_get_prefs (ph, cstr, &cstrret, &intret);
	JS_free(context, cstr);

	switch (cret)
	{
		case 0: // fail
			JS_SET_RVAL (context, vp, JSVAL_VOID);
			break;

		case 1: // string
			ret = JS_NewStringCopyZ (context, cstrret);
			JS_SET_RVAL (context, vp, STRING_TO_JSVAL(ret));
			break;

		case 2: // int
			JS_SET_RVAL (context, vp, INT_TO_JSVAL(intret));
			break;

		case 3: // bool
			JS_SET_RVAL (context, vp, BOOLEAN_TO_JSVAL(intret));
			break;
	}

	return JS_TRUE;
}

static JSBool
hjs_getlist (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* list_name;
	JSObject* js_list;
	const char* const *fields;
	const char* field;
	char* name;
	hexchat_list* list = nullptr;
	jsval iattr, sattr, tattr;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S", &list_name))
		return JS_FALSE;

	fields = hexchat_list_fields(ph, "lists");
	name = JSSTRING_TO_CHAR(list_name);
	for (int i = 0; fields[i]; i++)
	{
		if (strcmp (fields[i], name) == 0)
		{
			JS_free(context, name);
			name = (char*)fields[i];
			break;
		}
	}
	if (name == nullptr)
		goto listerr;

	js_list = JS_NewArrayObject (context, 0, nullptr);
	if (js_list == nullptr)
		goto listerr;

	list = hexchat_list_get (ph, name);
	if (list == nullptr)
		goto listerr;

	fields = hexchat_list_fields (ph, name);
	for (int index = 0; hexchat_list_next (ph, list); index++)
	{
		JSObject* list_obj = JS_NewObject (context, &list_entry_class, nullptr, nullptr);
		if (list_obj == nullptr)
			goto listerr;

		for (int i = 0; fields[i]; i++)
		{
			field = fields[i]+1;
			switch(fields[i][0])
			{
				case 's': // string
					sattr = STRING_TO_JSVAL(JS_NewStringCopyZ (context, hexchat_list_str (ph, list, field)));
					if (!JS_DefineProperty (context, list_obj, field, sattr,
											nullptr, nullptr, JSPROP_READONLY|JSPROP_ENUMERATE))
							goto listerr;
					break;

				case 'i': // int
					iattr = INT_TO_JSVAL(hexchat_list_int (ph, list, field));
					if (!JS_DefineProperty (context, list_obj, field, iattr,
										nullptr, nullptr, JSPROP_READONLY|JSPROP_ENUMERATE))
							goto listerr;
					break;

				case 't': // time
					tattr = hjs_util_datefromtime (context, hexchat_list_time (ph, list, field));
					if (!JS_DefineProperty (context, list_obj, field, tattr,
										nullptr, nullptr, JSPROP_READONLY|JSPROP_ENUMERATE))
							goto listerr;
					break;

				case 'p': // pointer
					// don't handle this for now, can they even be used?
					break;
			}
		}
		JS_DefineElement (context, js_list, index, OBJECT_TO_JSVAL(list_obj), nullptr, nullptr,
						JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_ENUMERATE);
	}

	hexchat_list_free (ph, list);

	JS_SET_RVAL (context, vp, OBJECT_TO_JSVAL(js_list));

	return JS_TRUE;

	listerr:
		if (list)
			hexchat_list_free (ph, list);

		JS_SET_RVAL (context, vp, JSVAL_VOID);
		return JS_FALSE;
}

static JSBool
hjs_findcontext (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* network = nullptr;
	JSString* channel = nullptr;
	char* cnetwork = nullptr;
	char* cchannel = nullptr;
	jsval ret;
	hexchat_context* ctx;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "/SS", &network, &channel))
		return JS_FALSE;

	if (network)
		cnetwork = JSSTRING_TO_CHAR(network);
	if (channel)
		cchannel = JSSTRING_TO_CHAR(channel);

	ctx = hexchat_find_context (ph, cnetwork, cchannel);

	if (cnetwork)
		JS_free(context, cnetwork);
	if (cchannel)
		JS_free(context, cchannel);

	if (!ctx)
		JS_SET_RVAL (context, vp, JSVAL_NULL);
	else if (!JS_NewNumberValue(context, (long)ctx, &ret))
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	else
		JS_SET_RVAL (context, vp, ret);

	return JS_TRUE;
}

static JSBool
hjs_getcontext (JSContext *context, unsigned argc, jsval *vp)
{
	hexchat_context* ctx;
	jsval ret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), ""))
		return JS_FALSE;

	ctx = hexchat_get_context (ph);

	if (!JS_NewNumberValue(context, (long)ctx, &ret))
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	else
		JS_SET_RVAL (context, vp, ret);

	return JS_TRUE;
}

static JSBool
hjs_setcontext (JSContext *context, unsigned argc, jsval *vp)
{
	long ctxnum;
	hexchat_context* ctx;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "u", &ctxnum))
		return JS_FALSE;

	ctx = (hexchat_context*)ctxnum;

	if (hexchat_set_context (ph, ctx))
		JS_SET_RVAL (context, vp, JSVAL_TRUE);
	else
		JS_SET_RVAL (context, vp, JSVAL_FALSE);

	return JS_TRUE;
}

static JSBool
hjs_hookcmd (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* cmdstr;
	JSString* helpstr = nullptr;
	JSObject* funcobj;
	JSObject* userdata = nullptr;
	jsval ret;
	char* chelpstr = nullptr;
	char* ccmdstr;
	int pri = HEXCHAT_PRI_NORM;
	hexchat_hook* hexhook;
	script_hook* hook = new script_hook;
	js_script* script = hjs_script_find (context);

	// these are slightly out of normal order, does jsapi have kwargs?
	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "So/Soi",
							&cmdstr, &funcobj, &helpstr, &userdata, &pri))
		return JS_FALSE;

	if (!JS_ObjectIsFunction (context, funcobj))
		return JS_FALSE;

	ccmdstr = JSSTRING_TO_CHAR(cmdstr);
	if (helpstr)
		chelpstr = JSSTRING_TO_CHAR(helpstr);

	hexhook = hexchat_hook_command (ph, ccmdstr, pri, hjs_callback, helpstr ? chelpstr : "", hook);

	JS_free(context, ccmdstr);
	if (chelpstr)
		JS_free(context, chelpstr);

	script->add_hook (hook, HOOK_CMD, context, funcobj, userdata, hexhook);

	if (!JS_NewNumberValue(context, (long)hexhook, &ret))
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	else
		JS_SET_RVAL (context, vp, ret);

	return JS_TRUE;
}

static JSBool
hjs_hookprint (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* event;
	JSObject* funcobj;
	JSObject* userdata = nullptr;
	jsval ret;
	char* cevent;
	int pri = HEXCHAT_PRI_NORM;
	hexchat_hook* hexhook;
	script_hook* hook = new script_hook;
	js_script* script = hjs_script_find (context);

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "So/oi",
							&event, &funcobj, &userdata, &pri))
		return JS_FALSE;

	if (!JS_ObjectIsFunction (context, funcobj))
		return JS_FALSE;

	cevent = JSSTRING_TO_CHAR(event);
	hexhook = hexchat_hook_print_attrs (ph, cevent, pri, hjs_callback, hook);
	JS_free(context, cevent);

	script->add_hook (hook, HOOK_PRINT, context, funcobj, userdata, hexhook);

	if (!JS_NewNumberValue(context, (long)hexhook, &ret))
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	else
		JS_SET_RVAL (context, vp, ret);

	return JS_TRUE;
}

static JSBool
hjs_hookspecial (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* event;
	JSObject* funcobj;
	JSObject* userdata = nullptr;
	jsval ret;
	char* cevent;
	int pri = HEXCHAT_PRI_NORM;
	hexchat_hook* hexhook;
	script_hook* hook = new script_hook;
	js_script* script = hjs_script_find (context);

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "So/oi",
							&event, &funcobj, &userdata, &pri))
		return JS_FALSE;

	if (!JS_ObjectIsFunction (context, funcobj))
		return JS_FALSE;

	/* This is technically the same as hook_print except that hook_print_attrs won't work with
	 * the "special" hooks, so to avoid confusion or adding another hook_print for attrs
	 * just create a new function hook_special */
	cevent = JSSTRING_TO_CHAR(event);
	hexhook = hexchat_hook_print (ph, cevent, pri, hjs_callback, hook);
	JS_free(context, cevent);

	script->add_hook (hook, HOOK_PRINT, context, funcobj, userdata, hexhook);

	if (!JS_NewNumberValue(context, (long)hexhook, &ret))
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	else
		JS_SET_RVAL (context, vp, ret);

	return JS_TRUE;
}

static JSBool
hjs_hookserver (JSContext *context, unsigned argc, jsval *vp)
{
	JSString* serverstr;
	JSObject* funcobj;
	JSObject* userdata = nullptr;
	jsval ret;
	char* cserverstr;
	int pri = HEXCHAT_PRI_NORM;
	hexchat_hook* hexhook;
	script_hook* hook = new script_hook;
	js_script* script = hjs_script_find (context);

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "So/oi",
							&serverstr, &funcobj, &userdata, &pri))
		return JS_FALSE;

	if (!JS_ObjectIsFunction (context, funcobj))
		return JS_FALSE;

	cserverstr = JSSTRING_TO_CHAR(serverstr);
	hexhook = hexchat_hook_server_attrs (ph, cserverstr, pri, hjs_callback, hook);
	JS_free(context, cserverstr);

	script->add_hook (hook, HOOK_SERVER, context, funcobj, userdata, hexhook);

	if (!JS_NewNumberValue(context, (long)hexhook, &ret))
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	else
		JS_SET_RVAL (context, vp, ret);

	return JS_TRUE;
}

static JSBool
hjs_hooktimer (JSContext *context, unsigned argc, jsval *vp)
{
	JSObject* funcobj;
	JSObject* userdata = nullptr;
	jsval ret;
	int timeout;
	hexchat_hook* hexhook;
	script_hook* hook = new script_hook;
	js_script* script = hjs_script_find (context);

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "io/o",
							&timeout, &funcobj, &userdata))
		return JS_FALSE;

	if (!JS_ObjectIsFunction (context, funcobj))
		return JS_FALSE;

	hexhook = hexchat_hook_timer (ph, timeout, hjs_callback, hook);

	script->add_hook (hook, HOOK_TIMER, context, funcobj, userdata, hexhook);

	if (!JS_NewNumberValue(context, (long)hexhook, &ret))
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	else
		JS_SET_RVAL (context, vp, ret);

	return JS_TRUE;
}

static JSBool
hjs_hookunload (JSContext *context, unsigned argc, jsval *vp)
{
	JSObject* funcobj;
	JSObject* userdata = nullptr;
	script_hook* hook = new script_hook;
	js_script* script = hjs_script_find (context);

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "o/o", &funcobj, &userdata))
		return JS_FALSE;

	script->add_hook (hook, HOOK_UNLOAD, context, funcobj, userdata, nullptr);

	JS_SET_RVAL (context, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool
hjs_unhook (JSContext *context, unsigned argc, jsval *vp)
{
	long hooknum;
	js_script* script;
	script_hook* hook;
	hexchat_hook* hexhook;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "u", &hooknum))
		return JS_FALSE;

	hexhook = (hexchat_hook*)hooknum;
	// unhook returns your original userdata
	hook =  (script_hook*)hexchat_unhook (ph, hexhook);

	script = hjs_script_find (context);
	script->remove_hook (hook);


	JS_SET_RVAL (context, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool
hjs_setpluginpref (JSContext *context, unsigned argc, jsval *vp)
{
	hexchat_plugin* prefph = hjs_script_gethandle (context); // fake handle to save to own conf file
	JSString* var;
	JSString* val;
	char* cvar;
	char* cval;
	int ret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "SS", &var, &val))
		return JS_FALSE;

	cvar = JSSTRING_TO_CHAR(var);
	cval = JSSTRING_TO_CHAR(val);

	// it is always stored as a string anyway.
	ret = hexchat_pluginpref_set_str (prefph, cvar, cval);

	JS_free(context, cvar);
	JS_free(context, cval);

	JS_SET_RVAL (context, vp, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
hjs_delpluginpref (JSContext *context, unsigned argc, jsval *vp)
{
	hexchat_plugin* prefph = hjs_script_gethandle (context);
	JSString* var;
	char* cvar;
	int ret;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S", &var))
		return JS_FALSE;

	cvar = JSSTRING_TO_CHAR(var);
	ret = hexchat_pluginpref_delete (prefph, cvar);
	JS_free(context, cvar);

	JS_SET_RVAL (context, vp, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
hjs_listpluginpref (JSContext *context, unsigned argc, jsval *vp)
{
	hexchat_plugin* prefph = hjs_script_gethandle (context);
	JSObject* js_list;
	JSString* list_item;
	jsuint list_len;
	char list[512];
	char* token;
	int result;
	int index = 0;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), ""))
		return JS_FALSE;

	js_list = JS_NewArrayObject (context, 0, nullptr);
	if (js_list == nullptr)
	{
		JS_SET_RVAL (context, vp, JSVAL_VOID);
		return JS_FALSE;
	}

	result = hexchat_pluginpref_list (prefph, list);
	if (result)
	{
		token = strtok (list, ",");
		while (token != nullptr)
		{
			list_item = JS_NewStringCopyZ (context, token);
			JS_DefineElement (context, js_list, index, STRING_TO_JSVAL(list_item), nullptr, nullptr,
							JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_ENUMERATE);

			token = strtok (nullptr, ",");
			index++;
		}
	}

	JS_GetArrayLength (context, js_list, &list_len);
	if (list_len)
		JS_SET_RVAL (context, vp, OBJECT_TO_JSVAL(js_list));
	else
		JS_SET_RVAL (context, vp, JSVAL_NULL);

	return JS_TRUE;
}

static JSBool
hjs_getpluginpref (JSContext *context, unsigned argc, jsval *vp)
{
	hexchat_plugin* prefph = hjs_script_gethandle (context);
	JSString* var;
	JSString* retstr;
	char* cvar;
	char cretstr[512];
	int cret, cretint;

	if (!JS_ConvertArguments (context, argc, JS_ARGV(context, vp), "S", &var))
		return JS_FALSE;

	cvar = JSSTRING_TO_CHAR(var);
	cret = hexchat_pluginpref_get_str (prefph, JSSTRING_TO_CHAR(var), cretstr);
	JS_free(context, cvar);

	if (cret)
	{
		if (strlen (cretstr) <= 12)
		{
			cvar = JSSTRING_TO_CHAR(var);
			cretint = hexchat_pluginpref_get_int (prefph, cvar);
			JS_free(context, cvar);

			if ((cretint == 0) && (strcmp(cretstr, "0") != 0))
			{
				// Failed to make int and string is not "0", its a string.
				retstr = JS_NewStringCopyZ (context, cretstr);
				JS_SET_RVAL (context, vp, STRING_TO_JSVAL(retstr));
			}
			else
			{
				// Must be an int
				JS_SET_RVAL (context, vp, INT_TO_JSVAL(cretint));
			}
		}
		else
		{
			// It won't store ints this large
			retstr = JS_NewStringCopyZ (context, cretstr);
			JS_SET_RVAL (context, vp, STRING_TO_JSVAL(retstr));
		}
	}
	else
	{
		// Failed
		JS_SET_RVAL (context, vp, JSVAL_VOID);
	}


	return JS_TRUE;
}

static JSFunctionSpec hexchat_functions[] = {
	{"print", hjs_print, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{"emit_print", hjs_emitprint, 6, JSPROP_READONLY|JSPROP_PERMANENT},
	{"emit_print_at", hjs_emitprintat, 7, JSPROP_READONLY|JSPROP_PERMANENT},
	{"command", hjs_command, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{"nickcmp", hjs_nickcmp, 2, JSPROP_READONLY|JSPROP_PERMANENT},
	{"strip", hjs_strip, 2, JSPROP_READONLY|JSPROP_PERMANENT},
	{"get_info", hjs_getinfo, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{"get_prefs", hjs_getprefs, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{"hook_command", hjs_hookcmd, 5, JSPROP_READONLY|JSPROP_PERMANENT},
	{"hook_server", hjs_hookserver, 4, JSPROP_READONLY|JSPROP_PERMANENT},
	{"hook_timer", hjs_hooktimer, 3, JSPROP_READONLY|JSPROP_PERMANENT},
	{"hook_print", hjs_hookprint, 5, JSPROP_READONLY|JSPROP_PERMANENT},
	{"hook_special", hjs_hookspecial, 5, JSPROP_READONLY|JSPROP_PERMANENT},
	{"hook_unload", hjs_hookunload, 2, JSPROP_READONLY|JSPROP_PERMANENT},
	{"unhook", hjs_unhook, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{"get_list", hjs_getlist, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{"find_context", hjs_findcontext, 2, JSPROP_READONLY|JSPROP_PERMANENT},
	{"get_context", hjs_getcontext, 0, JSPROP_READONLY|JSPROP_PERMANENT},
	{"set_context", hjs_setcontext, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{"set_pluginpref", hjs_setpluginpref, 2, JSPROP_READONLY|JSPROP_PERMANENT},
	{"get_pluginpref", hjs_getpluginpref, 2, JSPROP_READONLY|JSPROP_PERMANENT},
	{"list_pluginpref", hjs_listpluginpref, 0, JSPROP_READONLY|JSPROP_PERMANENT},
	{"del_pluginpref", hjs_delpluginpref, 1, JSPROP_READONLY|JSPROP_PERMANENT},
	{0}
};


/* init and deinit of JS */

static int
js_init (JSContext **cx, JSRuntime **rt, JSObject **globals, bool fake)
{
	// 1MB per runtime, unsure how much is actually needed for such basic scripts
	*rt = JS_NewRuntime (1024 * 1024);
	if (*rt == nullptr)
		return 0;

	*cx = JS_NewContext (*rt, 8192);
	if (*cx == nullptr)
		return 0;
	JS_SetOptions (*cx, JSOPTION_VAROBJFIX);
	JS_SetVersion (*cx, JSVERSION_LATEST);

	*globals = JS_NewCompartmentAndGlobalObject (*cx, &global_class, nullptr);
	if (*globals == nullptr)
		return 0;

	if (!JS_InitStandardClasses (*cx, *globals))
		return 0;

	if (!fake)
	{
		JS_SetErrorReporter (*cx, hjs_print_error);

		if (!JS_DefineFunctions (*cx, *globals, hexchat_functions))
			return 0;

		if (!(DEFINE_GLOBAL_PROP("VERSION", DOUBLE_TO_JSVAL(HJS_VERSION_FLOAT))
			&& DEFINE_GLOBAL_PROP("EAT_NONE", INT_TO_JSVAL(HEXCHAT_EAT_NONE))
			&& DEFINE_GLOBAL_PROP("EAT_HEXCHAT", INT_TO_JSVAL(HEXCHAT_EAT_HEXCHAT))
			&& DEFINE_GLOBAL_PROP("EAT_ALL", INT_TO_JSVAL(HEXCHAT_EAT_ALL))
			&& DEFINE_GLOBAL_PROP("STRIP_NONE", INT_TO_JSVAL(0))
			&& DEFINE_GLOBAL_PROP("STRIP_COLOR", INT_TO_JSVAL(1))
			&& DEFINE_GLOBAL_PROP("STRIP_ATTR", INT_TO_JSVAL(2))
			&& DEFINE_GLOBAL_PROP("STRIP_ALL", INT_TO_JSVAL(3))
			&& DEFINE_GLOBAL_PROP("PRI_HIGHEST", INT_TO_JSVAL(HEXCHAT_PRI_HIGHEST))
			&& DEFINE_GLOBAL_PROP("PRI_HIGH", INT_TO_JSVAL(HEXCHAT_PRI_HIGH))
			&& DEFINE_GLOBAL_PROP("PRI_NORM", INT_TO_JSVAL(HEXCHAT_PRI_NORM))
			&& DEFINE_GLOBAL_PROP("PRI_LOW", INT_TO_JSVAL(HEXCHAT_PRI_LOW))
			&& DEFINE_GLOBAL_PROP("PRI_LOWEST", INT_TO_JSVAL(HEXCHAT_PRI_LOWEST))))
				return 0;
	}

	return 1;
}

static void
js_deinit (JSContext *cx, JSRuntime *rt)
{
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
}

js_script::js_script (string file, string src)
{
	JSObject* fake_globals;
	JSContext* fake_context;
	JSRuntime* fake_runtime;

	js_script_list.push_back(this);

	filename = file;

	// create a fake runtime to get the scripts name without actually running it, is there an easier way?
	js_init (&fake_context, &fake_runtime, &fake_globals, true);
	JS_EvaluateScript (fake_context, fake_globals, src.c_str(), src.length(), file.c_str(), 0, nullptr);

	name = hjs_script_getproperty (fake_context, "SCRIPT_NAME", hjs_util_shrinkfile(file));
	desc = hjs_script_getproperty (fake_context, "SCRIPT_DESC", file);
	version = hjs_script_getproperty (fake_context, "SCRIPT_VER", "0");
	gui = hexchat_plugingui_add (ph, file.c_str(), name.c_str(), desc.c_str(), version.c_str(), nullptr);

	js_deinit (fake_context, fake_runtime);

	// now the real thing..
	js_init (&context, &runtime, &globals, false);
	JS_EvaluateScript (context, globals, src.c_str(), src.length(), file.c_str(), 0, nullptr);
}

void
js_script::add_hook (script_hook* hook, hook_type type, JSContext* context,
					JSObject* callback, JSObject* userdata, hexchat_hook* hexhook)
{
	hook->type = type;
	hook->context = context;
	hook->callback = callback;
	hook->userdata = userdata;
	hook->hook = hexhook;

	this->hooks.push_back(hook);
}

void
js_script::remove_hook (script_hook* hook)
{
	this->hooks.remove (hook);
	delete hook;
}

js_script::~js_script ()
{
	for (script_hook* hook : hooks)
	{
		if (hook->type == HOOK_UNLOAD)
		{
			JSFunction* fun = JS_ValueToFunction (hook->context, OBJECT_TO_JSVAL(hook->callback));
			jsval argv[] = { OBJECT_TO_JSVAL(hook->userdata) };
			jsval rval;

			JS_CallFunction (hook->context, JS_GetGlobalForScopeChain (hook->context), fun, 1, argv, &rval);
		}
		else
		{
			hexchat_unhook (ph, hook->hook);
		}

		delete hook;
	}

	js_deinit (context, runtime);

	if (gui != nullptr)
		hexchat_plugingui_remove(ph, gui);
}

extern "C"
{
	int
	hexchat_plugin_init (hexchat_plugin *plugin_handle, char* *plugin_name, char* *plugin_desc, char* *plugin_version, char* arg)
	{
		ph = plugin_handle;
		*plugin_name = name;
		*plugin_desc = description;
		*plugin_version = version;

		if (!js_init (&interp_cx, &interp_rt, &interp_globals, false))
			return 0;

		hexchat_hook_command (ph, "LOAD", HEXCHAT_PRI_NORM, hjs_load_cb, nullptr, nullptr);
		hexchat_hook_command (ph, "UNLOAD", HEXCHAT_PRI_NORM, hjs_unload_cb, nullptr, nullptr);
		hexchat_hook_command (ph, "RELOAD", HEXCHAT_PRI_NORM, hjs_reload_cb, nullptr, nullptr);
		hexchat_hook_command (ph, "JS", HEXCHAT_PRI_NORM, hjs_cmd_cb, help, nullptr);
		hexchat_printf (ph, "%s version %s loaded.\n", name, version);

		// allow avoiding autoload by passing anything
		if (arg == nullptr)
			hjs_script_autoload ();

		return 1;
	}
}

extern "C"
{
	int
	hexchat_plugin_deinit (hexchat_plugin *ph)
	{
		js_deinit (interp_cx, interp_rt);
		hjs_script_cleanup ();
		JS_ShutDown();
		hexchat_printf (ph, "%s version %s unloaded.\n", name, version);

		return 1;
	}
}
