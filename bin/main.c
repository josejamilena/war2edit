/*
 * Copyright (c) 2015-2016 Jean Guyomarc'h
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "war2edit.h"
#include <Ecore_Getopt.h>

static Eina_Bool _in_tree = EINA_FALSE;
static char *_edje_file = NULL;

static const Ecore_Getopt _options =
{
   "war2edit",
   "%prog [options] [PUD file(s)]",
   "0.1",
   "2014-2017 © Jean Guyomarc'h",
   "MIT",
   "A modest clone of Warcraft II World Map Editor",
   EINA_TRUE,
   {
      ECORE_GETOPT_STORE_TRUE('d', "debug", "Enable graphical debug"),
      ECORE_GETOPT_HELP ('h', "help"),
      ECORE_GETOPT_VERSION('V', "version"),
      ECORE_GETOPT_SENTINEL
   }
};

typedef struct
{
   const char *name;
   Eina_Bool (*init)(void);
   void (*shutdown)(void);
} Module;


static const Module _modules[] =
{
#define MODULE(name_) { #name_, name_ ## _init, name_ ## _shutdown }
   MODULE(log),
   MODULE(atlas),
   MODULE(sprite),
   MODULE(menu),
   MODULE(plugins),
   MODULE(editor)
#undef MODULE
};

static Eina_Bool
_edje_get(const char *theme)
{
   char path[PATH_MAX];
   int len;

   if (!theme) theme = "default";

   len = snprintf(path, sizeof(path), "%s/themes/%s.edj",
                  main_in_tree_is() ? BUILD_DATA_DIR : elm_app_data_dir_get(),
                  theme);
   path[sizeof(path) - 1] = '\0';

   _edje_file =  strndup(path, len);

   return (_edje_file == NULL) ? EINA_FALSE : EINA_TRUE;
}

EAPI_MAIN int
elm_main(int    argc,
         char **argv)
{
   int ret = EXIT_FAILURE;
   int args;
   int i;
   Editor *ed;
   unsigned int ed_count = 0;
   Eina_Bool quit_opt = EINA_FALSE;
   Eina_Bool debug = EINA_FALSE;
   Ecore_Getopt_Value values[] = {
      ECORE_GETOPT_VALUE_BOOL(debug),
      ECORE_GETOPT_VALUE_BOOL(quit_opt),
      ECORE_GETOPT_VALUE_BOOL(quit_opt)
   };
   const Module *mod_ptr;
   const Module *mod_end = &(_modules[EINA_C_ARRAY_LENGTH(_modules)]);
   const char *env;
   unsigned int debug_flags = 0;

   args = ecore_getopt_parse(&_options, values, argc, argv);
   if (args < 0)
     {
        EINA_LOG_CRIT("Getopt failed");
        goto end;
     }

   /* Quit option requested? End now, with success */
   if (quit_opt)
     {
        ret = EXIT_SUCCESS;
        goto end;
     }

   if (debug)
     debug_flags = ~0U;

   /* Are we running in tree? */
   env = getenv("WAR2EDIT_IN_TREE");
   _in_tree = (env) ? !!atoi(env) : EINA_FALSE;

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   elm_language_set("");
   elm_app_compile_bin_dir_set(PACKAGE_BIN_DIR);
   elm_app_compile_lib_dir_set(PACKAGE_LIB_DIR);
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(elm_main, "war2edit", "sprites/units/units.eet");

   if (EINA_UNLIKELY(!_edje_get(NULL)))
     {
        EINA_LOG_CRIT("Failed to get edje theme");
        goto end;
     }

   for (mod_ptr = _modules; mod_ptr != mod_end; ++mod_ptr)
     {
       if (EINA_UNLIKELY(EINA_FALSE == mod_ptr->init()))
         {
            EINA_LOG_CRIT("Failed to initialize module \"%s\"", mod_ptr->name);
            goto modules_shutdown;
         }
     }


   /* Open editors for each specified files */
   for (i = args; i < argc; ++i)
     {
        /* If an editor fails to open, don't close now */
        ed = editor_new(argv[i], debug_flags);
        if (!ed)
          ERR("Failed to create editor with file \"%s\"", argv[i]);
        else
          ++ed_count;
     }

   if (ed_count == 0)
     {
        ed = editor_new(NULL, debug_flags);
        if (EINA_UNLIKELY(!ed))
          {
             CRI("Failed to create editor");
             goto modules_shutdown;
          }
     }

   /* === Main loop === */
   elm_run();
   ret = EXIT_SUCCESS;

modules_shutdown:
   for (--mod_ptr; mod_ptr >= _modules; --mod_ptr)
     mod_ptr->shutdown();
end:
   if (_edje_file)
     {
        free(_edje_file);
        _edje_file = NULL;
     }
   return ret;
}
ELM_MAIN()

Eina_Bool
main_in_tree_is(void)
{
   return _in_tree;
}

const char *
main_edje_file_get(void)
{
   return _edje_file;
}
