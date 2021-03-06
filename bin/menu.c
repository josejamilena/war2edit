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

/*
 * Set to 1 to enable generated map debug.
 * On generation a grayscale PPM will be saved
 * in the current working directory
 */
#define DEGUG_GENERATOR 0

static Eina_Hash *_values = NULL;
#define PUD_DATA "war2edit/pud"


/*============================================================================*
 *                                 Private API                                *
 *============================================================================*/

#if DEGUG_GENERATOR
static void
_ppm_save(const float *map,
          unsigned int w,
          unsigned int h)
{
   FILE *f;
   const char filename[] = "randmap.ppm";
   unsigned int i, k = 0;
   const unsigned int size = w * h;

   f = fopen(filename, "w+");
   if (EINA_UNLIKELY(!f))
     {
        CRI("Failed to open \"%s\" (W+)", filename);
        return;
     }

   fprintf(f, "P2\n%u %u\n255\n", w, h);
   for (i = 0; i < size; i++)
     {
        fprintf(f, "%u ", (unsigned int)(map[k++] * 255.0f));
        if ((i + 1) % w == 0) fprintf(f, "\n");
     }
   fclose(f);
}

static void
_dump_map(const float *map,
          unsigned int w,
          unsigned int h)
{
   FILE *f;
   const char filename[] = "randmap.dat";
   const unsigned int size = w * h;
   unsigned int i;

   f = fopen(filename, "w+");
   if (EINA_UNLIKELY(!f))
     {
        CRI("Failed to open \"%s\"", filename);
        return;
     }

   for (i = 0; i < size; i++)
     fprintf(f, "%f %f\n", (double)i, (double)map[i]);
   fclose(f);
}
#endif

static Evas_Object *
_radio_add(Editor          *ed EINA_UNUSED,
           Evas_Object     *group,
           Evas_Object     *menu,
           unsigned int     object,
           Elm_Object_Item *parent,
           const char      *label,
           Evas_Smart_Cb    func,
           Eina_Array      *storage)
{
   Evas_Object *o;
   Elm_Object_Item *eoi;

   o = elm_radio_add(menu);
   elm_radio_state_value_set(o, object);
   if (group != NULL)
     elm_radio_group_add(o, group);

   if (label)
     {
        elm_object_text_set(o, label);
        eoi = elm_menu_item_add(menu, parent, NULL, NULL, func, o);
        elm_object_item_content_set(eoi, o);
        if (storage)
          eina_array_push(storage, eoi);
     }
   return o;
}

static void
_radio_changed_common_do(Evas_Object *radio,
                         int         *bind)
{
   int val;

   val = elm_radio_state_value_get(radio);
   elm_radio_value_set(radio, val);
   *bind = val;
}


/*============================================================================*
 *                                  Callbacks                                 *
 *============================================================================*/


static char *
_gen_units_text_get_cb(void        *data,
                       Evas_Object *obj  EINA_UNUSED,
                       const char  *part EINA_UNUSED)
{
   const Pud_Unit u = (Pud_Unit)((uintptr_t)data);
   return strdup(pud_unit_to_string(u, EINA_TRUE));
}

static Evas_Object *
_gen_units_icon_get_cb(void        *data,
                       Evas_Object *obj,
                       const char  *part)
{
   const Pud_Unit u = (Pud_Unit)((uintptr_t)data);
   Evas_Object *im = NULL;
   Editor *const ed = evas_object_data_get(obj, "editor");

   if (!strcmp(part, "elm.swallow.icon"))
     {
        im = editor_icon_image_new(obj, pud_unit_icon_get(u),
                                   ed->pud->era, PUD_PLAYER_RED);
     }
   return im;
}

static char *
_gen_upgrades_text_get_cb(void        *data,
                          Evas_Object *obj  EINA_UNUSED,
                          const char  *part EINA_UNUSED)
{
   const Pud_Upgrade u = (Pud_Upgrade)((uintptr_t)data);
   return strdup(pud_upgrade_to_string(u));
}

static Evas_Object *
_gen_upgrades_icon_get_cb(void        *data,
                          Evas_Object *obj,
                          const char  *part)
{
   const Pud_Upgrade u = (Pud_Upgrade)((uintptr_t)data);
   Evas_Object *im = NULL;
   Editor *const ed = evas_object_data_get(obj, "editor");

   if (!strcmp(part, "elm.swallow.icon"))
     {
        im = editor_icon_image_new(obj, pud_upgrade_icon_get(u),
                                   ed->pud->era, PUD_PLAYER_RED);
     }
   return im;
}

static void
_generator_func(Editor *ed,
                const Eina_Module *generator)
{
   Plugin_Generator_Func func;
   float *map;
   struct {
      Tile tile;
      float limit;
   } ctor[] = {
        { TILE_WATER_DARK,     -1.00f },
        { TILE_WATER_LIGHT,    -1.00f },
        { TILE_GROUND_LIGHT,   -1.00f },
        { TILE_GROUND_DARK,    -1.00f },
        { TILE_GRASS_LIGHT,     0.30f },
        { TILE_GRASS_DARK,      0.40f },
        { TILE_TREES,           0.60f },
        { TILE_GROUND_LIGHT,    0.65f },
        { TILE_ROCKS,           1.00f },
   };
   unsigned int i, j, k = 0, l;

   func = plugins_generator_func_get(generator);
   if (EINA_UNLIKELY(!func))
     {
        CRI("Failed to find symbol");
        return;
     }

   map = func(ed->pud->map_w, ed->pud->map_h, 0.1, 4);
   if (EINA_UNLIKELY(!map))
     {
        CRI("Failed to create random map");
        return;
     }

#if DEGUG_GENERATOR
   _ppm_save(map, ed->pud->map_w, ed->pud->map_h);
   _dump_map(map, ed->pud->map_w, ed->pud->map_h);
#endif

   /* Re-Init ctor */
   while (1)
     {
        for (l = 0; l < EINA_C_ARRAY_LENGTH(ctor); l++)
          ctor[l].limit = drand48();

        for (k = 0, l = 0; l < EINA_C_ARRAY_LENGTH(ctor) - 1; l++)
          {
             if (ctor[l].limit < ctor[l + 1].limit)
               k++;
          }
        if (k == EINA_C_ARRAY_LENGTH(ctor) - 1)
          break;
     }

#if DEGUG_GENERATOR
   printf("===\n");
   for (l = 0; l < EINA_C_ARRAY_LENGTH(ctor); l++)
     printf("%f\n", (double)ctor[l].limit);
#endif

   snapshot_push(ed);
   for (j = 0; j < ed->pud->map_h; j++)
     for (i = 0; i < ed->pud->map_w; i++)
       {
          for (l = 0; l < EINA_C_ARRAY_LENGTH(ctor); l++)
            {
               if (map[k] <= ctor[l].limit)
                 {
                    bitmap_tile_set(ed, i, j,
                                    ctor[l].tile, ctor[l].tile,
                                    ctor[l].tile, ctor[l].tile,
                                    TILE_RANDOMIZE, EINA_TRUE);
                    bitmap_tile_calculate(ed, i, j, NULL);
                    break;
                 }
            }

          k++;
       }
   snapshot_push_done(ed);
   free(map);
}

static inline void
_render_unlock(Editor *ed)
{
   editor_tiles_sync(ed);
   bitmap_render_unlock(ed);
   bitmap_render_flush(ed);
   menu_map_properties_update(ed);
}

static void
_randomize_cb(void        *data,
              Evas_Object *obj   EINA_UNUSED,
              void        *event EINA_UNUSED)
{
   Editor *const ed = data;
   const Eina_Module *m;

   /* FIXME BAD */
   m = plugins_request("generators", "perlin");

   bitmap_render_lock(ed);
   _generator_func(ed, m);
   _render_unlock(ed);
}

static void
_map_properties_cb(void        *data,
                   Evas_Object *obj   EINA_UNUSED,
                   void        *event EINA_UNUSED)
{
   Editor *ed = data;

   editor_inwin_set(ed, menu_map_properties_new(ed, editor_inwin_add(ed)),
                    "minimal", "Close", NULL, NULL, NULL);
}

static void
_player_properties_cb(void        *data,
                      Evas_Object *obj   EINA_UNUSED,
                      void        *event EINA_UNUSED)
{
   Editor *const ed = data;

   editor_inwin_set(ed, menu_player_properties_new(ed, editor_inwin_add(ed)),
                    "minimal", "Close", NULL, NULL, NULL);
   menu_unit_selection_reset(ed);
   toolbar_actions_selected_set(ed, 0, EINA_TRUE);
}

static void
_starting_properties_cb(void        *data,
                        Evas_Object *obj   EINA_UNUSED,
                        void        *event EINA_UNUSED)
{
   Editor *const ed = data;

   editor_inwin_set(ed, menu_starting_properties_new(ed, editor_inwin_add(ed)),
                    "minimal", "Close", NULL, NULL, NULL);
}

static void
_units_properties_cb(void        *data,
                     Evas_Object *obj   EINA_UNUSED,
                     void        *event EINA_UNUSED)
{
   Editor *const ed = data;

   editor_inwin_set(ed, menu_units_properties_new(ed, editor_inwin_add(ed)),
                    "default", "Close", NULL, NULL, NULL);
}

static void
_upgrades_properties_cb(void        *data,
                        Evas_Object *obj   EINA_UNUSED,
                        void        *event EINA_UNUSED)
{
   Editor *const ed = data;

   editor_inwin_set(ed, menu_upgrades_properties_new(ed, editor_inwin_add(ed)),
                    "default", "Close", NULL, NULL, NULL);
}

static void
_allow_properties_cb(void        *data,
                     Evas_Object *obj   EINA_UNUSED,
                     void        *event EINA_UNUSED)
{
   Editor *const ed = data;

   editor_inwin_set(ed, menu_allow_properties_new(ed, editor_inwin_add(ed)),
                    "default", "Close", NULL, NULL, NULL);
}

static void
_radio_units_changed_cb(void        *data,
                        Evas_Object *obj,
                        void        *event EINA_UNUSED)
{
   unsigned int w, h;
   Editor *ed = evas_object_data_get(obj, "editor");
   _radio_changed_common_do(data, (int *)(&(ed->sel_unit)));
   DBG("Units selection changed: <%s>", pud_unit_to_string(ed->sel_unit, PUD_TRUE));

   sprite_tile_size_get(ed->sel_unit, &w, &h);
   bitmap_cursor_size_set(ed, (int)w, (int)h);
   bitmap_cursor_visibility_set(ed, EINA_TRUE);
   toolbar_actions_segment_unselect(ed);
   editor_sel_action_set(ed, 0);
}

static void
_radio_players_changed_cb(void        *data,
                          Evas_Object *obj,
                          void        *event EINA_UNUSED)
{
   Editor *const ed = evas_object_data_get(obj, "editor");

   _radio_changed_common_do(data, (int *)(&(ed->sel_player)));
   menu_unit_selection_reset(ed);
   DBG("Player selection changed: <%s>", pud_color_to_string(ed->sel_player));

   menu_units_side_enable(ed, ed->pud->side.players[ed->sel_player]);
   _radio_units_changed_cb(ed->radio_units_reset, ed->unitsmenu, NULL);
   toolbar_actions_selected_set(ed, 0, EINA_TRUE);
}

/*============================================================================*
 *                                 Public API                                 *
 *============================================================================*/

Eina_Bool
menu_units_add(Editor *ed)
{
   Evas_Object *rd;
   Elm_Object_Item *i = NULL;

   ed->unitsmenu = elm_menu_add(ed->win);
   evas_object_data_set(ed->unitsmenu, "editor", ed);

#define RADIO_ADD_COMMON(unit_, label_, storage_) \
   _radio_add(ed, rd, ed->unitsmenu, unit_, i, label_, _radio_units_changed_cb, storage_)

#define RADIO_ADD_HUMAN(unit_) RADIO_ADD_COMMON(unit_, pud_unit_to_string(unit_, PUD_TRUE), ed->human_menus)
#define RADIO_ADD_ORC(unit_) RADIO_ADD_COMMON(unit_, pud_unit_to_string(unit_, PUD_TRUE), ed->orc_menus)
#define RADIO_ADD_NEUTRAL(unit_) RADIO_ADD_COMMON(unit_, pud_unit_to_string(unit_, PUD_TRUE), NULL)
#define RADIO_ADD(unit_, label_) RADIO_ADD_COMMON(unit_, label_, NULL)

   rd = NULL; /* Unset radio group */
   rd = RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_START);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Human Air", NULL, NULL);
   eina_array_push(ed->human_menus, i);
   RADIO_ADD_HUMAN(PUD_UNIT_GNOMISH_FLYING_MACHINE);
   RADIO_ADD_HUMAN(PUD_UNIT_GRYPHON_RIDER);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Human Land", NULL, NULL);
   eina_array_push(ed->human_menus, i);
   RADIO_ADD_HUMAN(PUD_UNIT_PEASANT);
   RADIO_ADD_HUMAN(PUD_UNIT_ATTACK_PEASANT);
   RADIO_ADD_HUMAN(PUD_UNIT_FOOTMAN);
   RADIO_ADD_HUMAN(PUD_UNIT_ARCHER);
   RADIO_ADD_HUMAN(PUD_UNIT_KNIGHT);
   RADIO_ADD_HUMAN(PUD_UNIT_BALLISTA);
   RADIO_ADD_HUMAN(PUD_UNIT_DWARVES);
   RADIO_ADD_HUMAN(PUD_UNIT_MAGE);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Human Water", NULL, NULL);
   eina_array_push(ed->human_menus, i);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_TANKER);
   RADIO_ADD_HUMAN(PUD_UNIT_ELVEN_DESTROYER);
   RADIO_ADD_HUMAN(PUD_UNIT_BATTLESHIP);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_TRANSPORT);
   RADIO_ADD_HUMAN(PUD_UNIT_GNOMISH_SUBMARINE);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Human Buildings", NULL, NULL);
   eina_array_push(ed->human_menus, i);
   RADIO_ADD_HUMAN(PUD_UNIT_FARM);
   RADIO_ADD_HUMAN(PUD_UNIT_TOWN_HALL);
   RADIO_ADD_HUMAN(PUD_UNIT_KEEP);
   RADIO_ADD_HUMAN(PUD_UNIT_CASTLE);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_BARRACKS);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_SHIPYARD);
   RADIO_ADD_HUMAN(PUD_UNIT_ELVEN_LUMBER_MILL);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_FOUNDRY);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_REFINERY);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_OIL_WELL);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_BLACKSMITH);
   RADIO_ADD_HUMAN(PUD_UNIT_STABLES);
   RADIO_ADD_HUMAN(PUD_UNIT_CHURCH);
   RADIO_ADD_HUMAN(PUD_UNIT_GNOMISH_INVENTOR);
   RADIO_ADD_HUMAN(PUD_UNIT_GRYPHON_AVIARY);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_SCOUT_TOWER);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_GUARD_TOWER);
   RADIO_ADD_HUMAN(PUD_UNIT_HUMAN_CANNON_TOWER);
   RADIO_ADD_HUMAN(PUD_UNIT_MAGE_TOWER);

   elm_menu_item_separator_add(ed->unitsmenu, NULL);

   i = NULL;
   RADIO_ADD_ORC(PUD_UNIT_ORC_START);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Orc Air", NULL, NULL);
   eina_array_push(ed->orc_menus, i);
   RADIO_ADD_ORC(PUD_UNIT_GOBLIN_ZEPPLIN);
   RADIO_ADD_ORC(PUD_UNIT_DRAGON);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Orc Land", NULL, NULL);
   eina_array_push(ed->orc_menus, i);
   RADIO_ADD_ORC(PUD_UNIT_PEON);
   RADIO_ADD_ORC(PUD_UNIT_ATTACK_PEON);
   RADIO_ADD_ORC(PUD_UNIT_GRUNT);
   RADIO_ADD_ORC(PUD_UNIT_AXETHROWER);
   RADIO_ADD_ORC(PUD_UNIT_OGRE);
   RADIO_ADD_ORC(PUD_UNIT_CATAPULT);
   RADIO_ADD_ORC(PUD_UNIT_GOBLIN_SAPPER);
   RADIO_ADD_ORC(PUD_UNIT_DEATH_KNIGHT);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Orc Water", NULL, NULL);
   eina_array_push(ed->orc_menus, i);
   RADIO_ADD_ORC(PUD_UNIT_ORC_TANKER);
   RADIO_ADD_ORC(PUD_UNIT_TROLL_DESTROYER);
   RADIO_ADD_ORC(PUD_UNIT_JUGGERNAUGHT);
   RADIO_ADD_ORC(PUD_UNIT_ORC_TRANSPORT);
   RADIO_ADD_ORC(PUD_UNIT_GIANT_TURTLE);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Orc Buildings", NULL, NULL);
   eina_array_push(ed->orc_menus, i);
   RADIO_ADD_ORC(PUD_UNIT_PIG_FARM);
   RADIO_ADD_ORC(PUD_UNIT_GREAT_HALL);
   RADIO_ADD_ORC(PUD_UNIT_STRONGHOLD);
   RADIO_ADD_ORC(PUD_UNIT_FORTRESS);
   RADIO_ADD_ORC(PUD_UNIT_ORC_BARRACKS);
   RADIO_ADD_ORC(PUD_UNIT_ORC_SHIPYARD);
   RADIO_ADD_ORC(PUD_UNIT_TROLL_LUMBER_MILL);
   RADIO_ADD_ORC(PUD_UNIT_ORC_FOUNDRY);
   RADIO_ADD_ORC(PUD_UNIT_ORC_REFINERY);
   RADIO_ADD_ORC(PUD_UNIT_ORC_OIL_WELL);
   RADIO_ADD_ORC(PUD_UNIT_ORC_BLACKSMITH);
   RADIO_ADD_ORC(PUD_UNIT_OGRE_MOUND);
   RADIO_ADD_ORC(PUD_UNIT_ALTAR_OF_STORMS);
   RADIO_ADD_ORC(PUD_UNIT_GOBLIN_ALCHEMIST);
   RADIO_ADD_ORC(PUD_UNIT_DRAGON_ROOST);
   RADIO_ADD_ORC(PUD_UNIT_ORC_SCOUT_TOWER);
   RADIO_ADD_ORC(PUD_UNIT_ORC_GUARD_TOWER);
   RADIO_ADD_ORC(PUD_UNIT_ORC_CANNON_TOWER);
   RADIO_ADD_ORC(PUD_UNIT_TEMPLE_OF_THE_DAMNED);

   elm_menu_item_separator_add(ed->unitsmenu, NULL);

   i = NULL;
   RADIO_ADD_NEUTRAL(PUD_UNIT_GOLD_MINE);
   RADIO_ADD_NEUTRAL(PUD_UNIT_OIL_PATCH);
   RADIO_ADD_NEUTRAL(PUD_UNIT_CRITTER);
   RADIO_ADD_NEUTRAL(PUD_UNIT_CIRCLE_OF_POWER);
   RADIO_ADD_NEUTRAL(PUD_UNIT_DARK_PORTAL);
   RADIO_ADD_NEUTRAL(PUD_UNIT_RUNESTONE);

   elm_menu_item_separator_add(ed->unitsmenu, NULL);

   RADIO_ADD_NEUTRAL(PUD_UNIT_SKELETON);
   RADIO_ADD_NEUTRAL(PUD_UNIT_DAEMON);

   elm_menu_item_separator_add(ed->unitsmenu, NULL);

   i = elm_menu_item_add(ed->unitsmenu, NULL, NULL, "Heroes", NULL, NULL);

   RADIO_ADD_ORC(PUD_UNIT_CHO_GALL);
   RADIO_ADD_ORC(PUD_UNIT_ZUL_JIN);
   RADIO_ADD_ORC(PUD_UNIT_GUL_DAN);
   RADIO_ADD_ORC(PUD_UNIT_GROM_HELLSCREAM);
   RADIO_ADD_ORC(PUD_UNIT_KARGATH_BLADEFIST);
   RADIO_ADD_ORC(PUD_UNIT_DENTARG);
   RADIO_ADD_ORC(PUD_UNIT_TERON_GOREFIEND);
   RADIO_ADD_ORC(PUD_UNIT_DEATHWING);

   elm_menu_item_separator_add(ed->unitsmenu, i);

   RADIO_ADD_HUMAN(PUD_UNIT_LOTHAR);
   RADIO_ADD_HUMAN(PUD_UNIT_UTHER_LIGHTBRINGER);
   RADIO_ADD_HUMAN(PUD_UNIT_TURALYON);
   RADIO_ADD_HUMAN(PUD_UNIT_ALLERIA);
   RADIO_ADD_HUMAN(PUD_UNIT_DANATH);
   RADIO_ADD_HUMAN(PUD_UNIT_KHADGAR);
   RADIO_ADD_HUMAN(PUD_UNIT_KURDRAN_AND_SKY_REE);


   /* Add a fictive radio which will be used to reset the units selection */
   ed->radio_units_reset = _radio_add(ed, rd, ed->unitsmenu, PUD_UNIT_NONE,
                                      NULL, NULL, _radio_units_changed_cb, NULL);
   menu_unit_selection_reset(ed);

#undef RADIO_ADD
#undef RADIO_ADD_HUMAN
#undef RADIO_ADD_ORC
#undef RADIO_ADD_COMMON


   return EINA_TRUE;
}

Eina_Bool
menu_players_add(Editor *ed)
{
   Evas_Object *rd;

   ed->playersmenu = elm_menu_add(ed->win);
   evas_object_data_set(ed->playersmenu, "editor", ed);

#define RADIO_ADD(unit_, label_) \
   _radio_add(ed, rd, ed->playersmenu, unit_, NULL, label_, _radio_players_changed_cb, NULL)

   rd = NULL; /* Reset the radio group */
   rd = RADIO_ADD(PUD_PLAYER_RED, STR_PLAYER_1);
   RADIO_ADD(PUD_PLAYER_BLUE,     STR_PLAYER_2);
   RADIO_ADD(PUD_PLAYER_GREEN,    STR_PLAYER_3);
   RADIO_ADD(PUD_PLAYER_VIOLET,   STR_PLAYER_4);
   RADIO_ADD(PUD_PLAYER_ORANGE,   STR_PLAYER_5);
   RADIO_ADD(PUD_PLAYER_BLACK,    STR_PLAYER_6);
   RADIO_ADD(PUD_PLAYER_WHITE,    STR_PLAYER_7);
   RADIO_ADD(PUD_PLAYER_YELLOW,   STR_PLAYER_8);

#undef RADIO_ADD

   return EINA_TRUE;
}

Eina_Bool
menu_properties_add(Editor *ed)
{
   Evas_Object *m;

   m = ed->propertiesmenu = elm_menu_add(ed->win);
   elm_menu_item_add(m, NULL, NULL, "Map Properties...", _map_properties_cb, ed);
   elm_menu_item_add(m, NULL, NULL, "Player Properties...", _player_properties_cb, ed);
   elm_menu_item_add(m, NULL, NULL, "Starting Properties...", _starting_properties_cb, ed);

   elm_menu_item_add(m, NULL, NULL, "Units Properties...", _units_properties_cb, ed);
   elm_menu_item_add(m, NULL, NULL, "Upgrades Properties...", _upgrades_properties_cb, ed);
   elm_menu_item_add(m, NULL, NULL, "Allow Properties...", _allow_properties_cb, ed);

   return EINA_TRUE;
}

void
menu_units_side_enable(Editor   *ed,
                       Pud_Side  enable)
{
   Elm_Object_Item *menu;
   Eina_Array_Iterator iterator;
   unsigned int i;
   const Eina_Bool disable_orcs = (enable == PUD_SIDE_HUMAN);
   const Eina_Bool disable_humans = !disable_orcs;

   EINA_ARRAY_ITER_NEXT(ed->orc_menus, i, menu, iterator)
      elm_object_item_disabled_set(menu, disable_orcs);

   EINA_ARRAY_ITER_NEXT(ed->human_menus, i, menu, iterator)
      elm_object_item_disabled_set(menu, disable_humans);
}

void
menu_unit_selection_reset(Editor *ed)
{
   elm_radio_value_set(ed->radio_units_reset, PUD_UNIT_NONE);
   ed->sel_unit = PUD_UNIT_NONE;
}

/*============================================================================*
 *                               Map Properties                               *
 *============================================================================*/

static Evas_Object *
_table_add(Evas_Object *obj)
{
   Evas_Object *t;

   t = elm_table_add(obj);
   evas_object_size_hint_weight_set(t, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(t, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_homogeneous_set(t, EINA_TRUE);
   elm_table_padding_set(t, 20, 6);
   evas_object_show(t);

   return t;
}

static Evas_Object *
_frame_add(Evas_Object *parent,
           const char  *text)
{
   Evas_Object *f;

   f = elm_frame_add(parent);
   elm_object_text_set(f, text);
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(f, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(f);

   return f;
}

void
menu_map_properties_update(Editor *ed)
{
   unsigned char *px;
   int w, h;

   px = minimap_pixels_get(ed, &w, &h);
   evas_object_image_size_set(ed->preview.obj, w, h);
   evas_object_image_data_set(ed->preview.obj, px);
   evas_object_image_data_update_add(ed->preview.obj, 0, 0, w, h);

   evas_object_size_hint_min_set(ed->preview.obj, 256, 256);
   evas_object_size_hint_max_set(ed->preview.obj, 256, 256);
   evas_object_show(ed->preview.obj);
}

static void
_era_changed_cb(void        *data,
                Evas_Object *obj,
                void        *event_info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Era era;

   era = elm_radio_value_get(obj);

   /* FIXME
    * Refcount atlases, and close old ones
    */
   atlas_texture_open(era);
   atlas_icon_open(era);
   sprite_buildings_open(era);

   pud_era_set(ed->pud, era);

   if (ed->bitmap.img)
     bitmap_refresh(ed, NULL);
   if (ed->minimap.map)
     minimap_reload(ed);

   menu_map_properties_update(ed);
}

static void
_set_description_cb(void        *data,
                    Evas_Object *obj,
                    void        *event_info EINA_UNUSED)
{
   Editor *const ed = data;

   pud_description_set(ed->pud, elm_object_text_get(obj));
}

Evas_Object *
menu_map_properties_new(Editor      *ed,
                        Evas_Object *parent)
{
   Evas_Object *f, *box, *b, *o, *bb, *ff, *e, *gens;
   static Elm_Entry_Filter_Limit_Size limit = {
      .max_char_count = 31,
      .max_byte_count = 0,
   };

   /* Frame for map era */
   f = elm_frame_add(parent);
   elm_object_text_set(f, "Map Properties");
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(f, 0.5, 0.5);
   evas_object_show(f);

   /* Vertical box (MAP + TEXT) */
   bb = elm_box_add(f);
   evas_object_size_hint_weight_set(bb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(bb, EINA_FALSE);
   elm_box_padding_set(bb, 0, 5);
   evas_object_show(bb);

   /* Container */
   box = elm_box_add(f);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(box, EINA_TRUE);
   elm_box_padding_set(box, 5, 0);
   evas_object_show(box);

   b = elm_box_add(f); /* Box */
   evas_object_show(b);

   /* Tileset item 1 (Forest) */
   o = elm_radio_add(b);
   elm_radio_state_value_set(o, PUD_ERA_FOREST);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Forest");
   elm_box_pack_end(b, o);
   evas_object_show(o);
   ed->menu_map_radio_group = o;
   evas_object_smart_callback_add(o, "changed", _era_changed_cb, ed);

   /* Tileset item 2 (Winter) */
   o = elm_radio_add(b);
   elm_radio_state_value_set(o, PUD_ERA_WINTER);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Winter");
   elm_box_pack_end(b, o);
   evas_object_show(o);
   elm_radio_group_add(o, ed->menu_map_radio_group);
   evas_object_smart_callback_add(o, "changed", _era_changed_cb, ed);

   /* Tileset item 3 (Wasteland) */
   o = elm_radio_add(b);
   elm_radio_state_value_set(o, PUD_ERA_WASTELAND);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Wasteland");
   elm_box_pack_end(b, o);
   evas_object_show(o);
   elm_radio_group_add(o, ed->menu_map_radio_group);
   evas_object_smart_callback_add(o, "changed", _era_changed_cb, ed);

   /* Tileset item 4 (Swamp) */
   ed->menu_swamp_radio = o = elm_radio_add(b);
   elm_radio_state_value_set(o, PUD_ERA_SWAMP);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(o, 0.0, 0.5);
   elm_object_text_set(o, "Swamp");
   elm_box_pack_end(b, o);
   evas_object_show(o);
   evas_object_smart_callback_add(o, "changed", _era_changed_cb, ed);
   elm_radio_group_add(o, ed->menu_map_radio_group);

   /* Default value */
   elm_radio_value_set(ed->menu_map_radio_group, PUD_ERA_FOREST);
   ed->extension = EINA_TRUE; // FIXME ???

   /* Map preview */
   ed->preview.obj = evas_object_image_filled_add(evas_object_evas_get(box));
   evas_object_image_colorspace_set(ed->preview.obj, EVAS_COLORSPACE_ARGB8888);
   evas_object_size_hint_weight_set(ed->preview.obj, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ed->preview.obj, 0.0, EVAS_HINT_FILL);
   menu_map_properties_update(ed);

   /* */
     {
        Evas_Object *o, *b;

        gens = elm_frame_add(box);
        evas_object_size_hint_weight_set(gens, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(gens, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_object_text_set(gens, "Generators");
        evas_object_show(gens);

        b = elm_box_add(gens);
        evas_object_size_hint_weight_set(b, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(b, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_box_horizontal_set(b, EINA_FALSE);
        evas_object_show(b);
        elm_object_content_set(gens, b);

        o = elm_button_add(b);
        evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 1.0);
        evas_object_show(o);
        elm_object_text_set(o, "Random Map");
        evas_object_smart_callback_add(o, "clicked", _randomize_cb, ed);
        elm_box_pack_end(b, o);
     }

   /* Compose the final box */
   elm_box_pack_start(box, b);
   elm_box_pack_end(box, ed->preview.obj);
   elm_box_pack_end(box, gens);

   ff = elm_frame_add(f);
   elm_object_text_set(ff, "Map Description");
   evas_object_size_hint_weight_set(ff, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ff, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(ff);

   e = elm_entry_add(ff);
   evas_object_size_hint_weight_set(e, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(e, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_markup_filter_append(e, elm_entry_filter_limit_size, &limit);
   evas_object_smart_callback_add(e, "changed,user", _set_description_cb, ed);
   elm_object_text_set(e, pud_description_get(ed->pud));
   elm_entry_single_line_set(e, EINA_TRUE);
   elm_entry_scrollable_set(e, EINA_TRUE);
   elm_entry_editable_set(e, EINA_TRUE);
   evas_object_show(e);
   elm_object_content_set(ff, e);

   elm_box_pack_start(bb, box);
   elm_box_pack_end(bb, ff);
   elm_object_content_set(f, bb);

   return f;
}

Eina_Bool
menu_init(void)
{
   static const uint8_t values[] = {
      PUD_SIDE_ORC,
      PUD_SIDE_HUMAN,
      PUD_OWNER_HUMAN,
      PUD_OWNER_COMPUTER,
      PUD_OWNER_RESCUE_PASSIVE,
      PUD_OWNER_RESCUE_ACTIVE,
      PUD_AI_LAND_ATTACK,
      PUD_AI_PASSIVE,
      PUD_AI_SEA_ATTACK,
      PUD_AI_AIR_ATTACK
   };
   Eina_Bool chk;
   Eina_Stringshare *shr;

   _values = eina_hash_stringshared_new(NULL);
   if (EINA_UNLIKELY(!_values))
     {
        CRI("Failed to create hash of stringshares");
        return EINA_FALSE;
     }

#define ADD(str, val) \
   do { \
      shr = eina_stringshare_add_length(str, sizeof(str) - 1); \
      if (EINA_UNLIKELY(!shr)) { \
         CRI("Failed to create stringshare for string \"" str "\""); \
         return EINA_FALSE; \
      } \
      chk = eina_hash_add(_values, shr, val); \
      if (EINA_UNLIKELY(!chk)) { \
         CRI("Failed to add value <0x%x> for stringshare \"%s\"", *val, shr); \
         eina_stringshare_del(shr); \
         eina_hash_free(_values); \
         return EINA_FALSE; \
      } \
   } while (0)

   ADD(STR_PUD_SIDE_ORC            , &(values[0]));
   ADD(STR_PUD_SIDE_HUMAN          , &(values[1]));
   ADD(STR_PUD_OWNER_HUMAN         , &(values[2]));
   ADD(STR_PUD_OWNER_COMPUTER      , &(values[3]));
   ADD(STR_PUD_OWNER_RESCUE_PASSIVE, &(values[4]));
   ADD(STR_PUD_OWNER_RESCUE_ACTIVE , &(values[5]));
   ADD(STR_PUD_AI_LAND_ATTACK      , &(values[7]));
   ADD(STR_PUD_AI_PASSIVE          , &(values[7]));
   ADD(STR_PUD_AI_SEA_ATTACK       , &(values[8]));
   ADD(STR_PUD_AI_AIR_ATTACK       , &(values[9]));

#undef ADD

   return EINA_TRUE;
}

void
menu_shutdown(void)
{
   eina_hash_free(_values);
}

/*============================================================================*
 *                             Players Properties                             *
 *============================================================================*/

static void
_hoversel_selected_cb(void        *data EINA_UNUSED,
                      Evas_Object *obj,
                      void        *info)
{
   elm_object_text_set(obj, elm_object_item_text_get(info));
}

static inline void
_hoversel_item_add(Evas_Object   *hoversel,
                   const char    *label,
                   Evas_Smart_Cb  func,
                   void          *bind)
{
   elm_hoversel_item_add(hoversel, label, NULL, ELM_ICON_NONE, func, bind);
}

static Evas_Object *
_pack_label(Evas_Object  *table,
            unsigned int  row,
            unsigned int  col,
            const char   *label)
{
   Evas_Object *o;

   o = elm_label_add(table);
   evas_object_size_hint_align_set(o, 0.0, EVAS_HINT_FILL);
   elm_object_text_set(o, label);
   evas_object_show(o);
   elm_table_pack(table, o, col, row, 1, 1);

   return o;
}

static Evas_Object *
_pack_label_right(Evas_Object *table,
                  unsigned int row,
                  unsigned int col,
                  const char   *label)
{
   Evas_Object *o;

   o = _pack_label(table, col, row, label);
   evas_object_size_hint_align_set(o, 1.0, EVAS_HINT_FILL);
   return o;
}

static void
_bind_cb(void        *data,
         Evas_Object *obj,
         void        *evt)
{
   uint8_t *bind = data;
   uint8_t *val, v;
   Eina_Stringshare *text;
   Editor *ed;
   Pud_Player player;

   text = elm_object_item_part_text_get(evt, "default");

   val = eina_hash_find(_values, text);
   if (EINA_UNLIKELY(!val))
     {
        CRI("Failed to get bind value for text \"%s\"", text);
        return;
     }
   v = *val;

   if ((v == PUD_SIDE_ORC) || (v == PUD_SIDE_HUMAN))
     {
        if (*bind != v)
          {
             *bind = v;
             ed = evas_object_data_get(obj, "editor");
             /* A little bit of black magic :-) */
             player = (Pud_Player)((unsigned char *)bind - (unsigned char *)(&ed->pud->side));
             editor_player_switch_race(ed, player);
          }
     }
   else
     *bind = v;
}

static void
_bind_side_cb(void        *data,
              Evas_Object *obj,
              void        *evt)
{
   Editor *ed;

   _bind_cb(data, obj, evt);

   ed = evas_object_data_get(obj, "editor");
   menu_units_side_enable(ed, ed->pud->side.players[ed->sel_player]);
}

static Evas_Object *
_hoversel_add(Evas_Object *parent,
              const char  *init_label)
{
   Evas_Object *o;

   o = elm_hoversel_add(parent);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_smart_callback_add(o, "selected", _hoversel_selected_cb, NULL);
   elm_hoversel_hover_parent_set(o, parent);
   elm_object_text_set(o, init_label);
   evas_object_show(o);

   return o;
}

static Evas_Object *
_pack_race_selector(Evas_Object  *table,
                    unsigned int  row,
                    unsigned int  col,
                    uint8_t      *bind)
{
   Evas_Object *o;
   const char *human_race = STR_PUD_SIDE_HUMAN;
   const char *orc_race = STR_PUD_SIDE_ORC;
   const char *race = (*bind == PUD_SIDE_HUMAN) ? human_race : orc_race;

   o = _hoversel_add(table, race);
   _hoversel_item_add(o, human_race, _bind_side_cb, bind);
   _hoversel_item_add(o, orc_race, _bind_side_cb, bind);

   elm_table_pack(table, o, col, row, 1, 1);
   return o;
}

static const char *
_owner_to_string(uint8_t owner)
{
   switch (owner)
     {
      case PUD_OWNER_NOBODY:         /* Fall through */
      case PUD_OWNER_COMPUTER:       return STR_PUD_OWNER_COMPUTER;
      case PUD_OWNER_HUMAN:          return STR_PUD_OWNER_HUMAN;
      case PUD_OWNER_RESCUE_PASSIVE: return STR_PUD_OWNER_RESCUE_PASSIVE;
      case PUD_OWNER_RESCUE_ACTIVE:  return STR_PUD_OWNER_RESCUE_ACTIVE;

      default:
         CRI("Unhandled value 0x%x", owner);
         return NULL;
     }
}

static void
_pack_owner_selector(Evas_Object  *table,
                     unsigned int  row,
                     unsigned int  col,
                     uint8_t      *bind)
{
   uint8_t values[] = {
      PUD_OWNER_COMPUTER,
      PUD_OWNER_HUMAN,
      PUD_OWNER_RESCUE_PASSIVE,
      PUD_OWNER_RESCUE_ACTIVE,
   };
   Evas_Object *o;
   unsigned int i;

   o = _hoversel_add(table, _owner_to_string(*bind));
   for (i = 0; i < EINA_C_ARRAY_LENGTH(values); ++i)
     _hoversel_item_add(o, _owner_to_string(values[i]), _bind_cb, bind);
   elm_table_pack(table, o, col, row, 1, 1);
}

static const char *
_ai_to_string(uint8_t ai)
{
   static char buf[32];

   switch (ai)
     {
      case PUD_AI_LAND_ATTACK: return STR_PUD_AI_LAND_ATTACK;
      case PUD_AI_SEA_ATTACK:  return STR_PUD_AI_SEA_ATTACK;
      case PUD_AI_AIR_ATTACK:  return STR_PUD_AI_AIR_ATTACK;
      case PUD_AI_PASSIVE:     return STR_PUD_AI_PASSIVE;

      /* Ignore these */
      case PUD_AI_HUMAN_14_RED:
      case PUD_AI_HUMAN_14_WHITE:
      case PUD_AI_HUMAN_14_BLACK:
      case PUD_AI_ORC_14_GREEN:
      case PUD_AI_ORC_14_WHITE:
         return NULL;

      default:
         break;
     }

   if ((ai >= PUD_AI_ORC_3) && (ai <= PUD_AI_ORC_13))
     {
        if (ai % 2 == 0) /* orc */
          snprintf(buf, sizeof(buf), STR_PUD_AI_HUMAN_FMT, ai / 2);
        else /* human */
          snprintf(buf, sizeof(buf), STR_PUD_AI_ORC_FMT, (ai / 2) + 1);
     }
   else if ((ai >= PUD_AI_EXPANSION_1) && (ai <= PUD_AI_EXPANSION_51))
     {
        snprintf(buf, sizeof(buf), STR_PUD_AI_EXPANSION_FMT,
                 ai - PUD_AI_EXPANSION_1 + 1);
     }
   else
     {
        CRI("Unhandled AI value 0x%x", ai);
        buf[0] = '?'; buf[1] = '?'; buf[2] = '?'; buf[3] = '\0';
     }

   buf[sizeof(buf) - 1] = '\0';
   return buf;
}

static void
_pack_ai_selector(Evas_Object  *table,
                  unsigned int  row,
                  unsigned int  col,
                  uint8_t      *bind)
{
   Evas_Object *o;
   unsigned int i;
   const uint8_t values[] = {
      PUD_AI_LAND_ATTACK,
      PUD_AI_PASSIVE,
      PUD_AI_SEA_ATTACK,
      PUD_AI_AIR_ATTACK,

      PUD_AI_ORC_3,
      PUD_AI_ORC_4,
      PUD_AI_ORC_5,
      PUD_AI_ORC_6,
      PUD_AI_ORC_7,
      PUD_AI_ORC_8,
      PUD_AI_ORC_9,
      PUD_AI_ORC_10,
      PUD_AI_ORC_11,
      PUD_AI_ORC_12,
      PUD_AI_ORC_13,
      PUD_AI_HUMAN_4,
      PUD_AI_HUMAN_5,
      PUD_AI_HUMAN_6,
      PUD_AI_HUMAN_7,
      PUD_AI_HUMAN_8,
      PUD_AI_HUMAN_9,
      PUD_AI_HUMAN_10,
      PUD_AI_HUMAN_11,
      PUD_AI_HUMAN_12,
      PUD_AI_HUMAN_13,
   };
   const unsigned int count = EINA_C_ARRAY_LENGTH(values);

   o = _hoversel_add(table, _ai_to_string(*bind));

   for (i = 0; i < count; ++i)
     _hoversel_item_add(o, _ai_to_string(values[i]), _bind_cb, bind);

   // More AIs. Slow!!
   //for (i = PUD_AI_ORC_4; i <= PUD_AI_HUMAN_6/*PUD_AI_ORC_13*/; ++i)
   //  ITEM_ADD(i);
   //for (i = PUD_AI_EXPANSION_1; i < PUD_AI_EXPANSION_51; ++i)
   //  ITEM_ADD(i);

   elm_table_pack(table, o, col, row, 1, 1);
}

Evas_Object *
menu_player_properties_new(Editor      *ed,
                           Evas_Object *parent)
{
   Evas_Object *f, *t, *o;
   unsigned int i;

   f = _frame_add(parent, "Players Properties");
   t = _table_add(f);
   elm_object_content_set(f, t);

   /* Players */
   _pack_label(t, 0, 0, "Player");
   _pack_label(t, 1, 0, STR_PLAYER_1);
   _pack_label(t, 2, 0, STR_PLAYER_2);
   _pack_label(t, 3, 0, STR_PLAYER_3);
   _pack_label(t, 4, 0, STR_PLAYER_4);
   _pack_label(t, 5, 0, STR_PLAYER_5);
   _pack_label(t, 6, 0, STR_PLAYER_6);
   _pack_label(t, 7, 0, STR_PLAYER_7);
   _pack_label(t, 8, 0, STR_PLAYER_8);

   /* Race */
   _pack_label(t, 0, 1, "Race");
   for (i = 0; i < 8; ++i)
     {
        o = _pack_race_selector(t, i + 1, 1, &(ed->pud->side.players[i]));
        evas_object_data_set(o, "editor", ed);
     }

   /* Owner */
   _pack_label(t, 0, 2, "Owner");
   for (i = 0; i < 8; ++i)
     _pack_owner_selector(t, i + 1, 2, &(ed->pud->owner.players[i]));

   /* AI */
   _pack_label(t, 0, 3, "AI");
   for (i = 0; i < 8; ++i)
     _pack_ai_selector(t, i + 1, 3, &(ed->pud->ai.players[i]));


   return f;
}


/*============================================================================*
 *                             Starting Properties                            *
 *============================================================================*/

static void
_validate_res_cb(void        *data,
                 Evas_Object *obj,
                 void        *info EINA_UNUSED)
{
   uint16_t *const bind = data;
   const char *text;
   unsigned long int val;
   char *ptr;

   text = elm_object_text_get(obj);
   val = strtoul(text, &ptr, 10);
   if ((text + strlen(text) != ptr) || (val > UINT16_MAX))
     {
        elm_layout_signal_emit(obj, "validation,default,fail", "elm");
     }
   else
     {
        elm_layout_signal_emit(obj, "validation,default,pass", "elm");
        *bind = val;
     }
}

static void
_pack_resource_entry(Evas_Object *table,
                     unsigned int row,
                     unsigned int col,
                     uint16_t     *bind)
{
   Evas_Object *o;
   char buf[32];

   o = elm_entry_add(table);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(o, EINA_TRUE);
   elm_entry_scrollable_set(o, EINA_TRUE);
   elm_entry_editable_set(o, EINA_TRUE);
   evas_object_show(o);

   snprintf(buf, sizeof(buf), "%u", *bind);
   buf[sizeof(buf) - 1] = '\0';

   elm_object_text_set(o, buf);

   evas_object_smart_callback_add(o, "changed,user", _validate_res_cb, bind);
   elm_table_pack(table, o, col, row, 1, 1);
}

Evas_Object *
menu_starting_properties_new(Editor      *ed,
                             Evas_Object *parent)
{
   Evas_Object *f, *t;
   unsigned int i;

   f = _frame_add(parent, "Starting Properties");
   t = _table_add(f);
   elm_object_content_set(f, t);

   /* Players */
   _pack_label(t, 0, 0, "Player");
   _pack_label(t, 1, 0, STR_PLAYER_1);
   _pack_label(t, 2, 0, STR_PLAYER_2);
   _pack_label(t, 3, 0, STR_PLAYER_3);
   _pack_label(t, 4, 0, STR_PLAYER_4);
   _pack_label(t, 5, 0, STR_PLAYER_5);
   _pack_label(t, 6, 0, STR_PLAYER_6);
   _pack_label(t, 7, 0, STR_PLAYER_7);
   _pack_label(t, 8, 0, STR_PLAYER_8);

   /* Gold */
   _pack_label(t, 0, 1, "Gold");
   for (i = 0; i < 8; ++i)
     _pack_resource_entry(t, i + 1, 1, &(ed->pud->sgld.players[i]));

   /* Lumber */
   _pack_label(t, 0, 2, "Lumber");
   for (i = 0; i < 8; ++i)
     _pack_resource_entry(t, i + 1, 2, &(ed->pud->slbr.players[i]));

   /* Oil */
   _pack_label(t, 0, 3, "Oil");
   for (i = 0; i < 8; ++i)
     _pack_resource_entry(t, i + 1, 3, &(ed->pud->soil.players[i]));

   return f;
}


/*============================================================================*
 *                              Units Properties                              *
 *============================================================================*/

static void
_unit_select_cb(void        *data,
                Evas_Object *obj  EINA_UNUSED,
                void        *evt)
{
   /*
    *
    * Initial state of units
    *
    */

   Editor *const ed = data;
   const Pud_Unit u = (Pud_Unit)((uintptr_t)elm_object_item_data_get(evt));
   Menu_Units *mu;
   Pud_Unit_Description *c;

   ed->menu_units->selected = u;
   mu = ed->menu_units;
   c = &(ed->pud->units_descr[u]);

   elm_spinner_value_set(mu->range, c->range);
   elm_spinner_value_set(mu->sight, c->sight);
   elm_spinner_value_set(mu->hp, c->hp);
   elm_spinner_value_set(mu->lumber, c->lumber_cost * 10);
   elm_spinner_value_set(mu->gold, c->gold_cost * 10);
   elm_spinner_value_set(mu->oil, c->oil_cost * 10);
   elm_spinner_value_set(mu->armor, c->armor);
   elm_spinner_value_set(mu->basic_damage, c->basic_damage);
   elm_spinner_value_set(mu->piercing_damage, c->piercing_damage);
   elm_check_state_set(mu->has_magic, !!c->has_magic);
   elm_check_state_set(mu->weapons_upgradable, !!c->weapons_upgradable);
   elm_check_state_set(mu->armor_upgradable, !!c->armor_upgradable);
   elm_slider_value_set(mu->time, c->build_time);
   elm_check_state_pointer_set(mu->has_magic, &c->has_magic);
   elm_check_state_pointer_set(mu->weapons_upgradable, &c->weapons_upgradable);
   elm_check_state_pointer_set(mu->armor_upgradable, &c->armor_upgradable);

   elm_object_text_set(mu->missile, pud_projectile_to_string(c->missile_weapon));
}

static Evas_Object *
_pack_ui(Editor *ed,
         Evas_Object *table,
         unsigned int row,
         const char *label,
         Evas_Object *(*ctor)(Evas_Object *o, Editor *ed, Evas_Smart_Cb cb),
         Evas_Smart_Cb cb)
{
   Evas_Object *o;

   _pack_label_right(table, 0, row, label);
   o = ctor(table, ed, cb);
   elm_table_pack(table, o, 1, row, 1, 1);

   return o;
}

static Evas_Object *
_pack_ui_check(Editor *ed EINA_UNUSED,
               Evas_Object *table,
               unsigned int row,
               const char *label)
{
   Evas_Object *o;

   _pack_label_right(table, 0, row, label);
   o = elm_check_add(table);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(o);
   elm_table_pack(table, o, 1, row, 1, 1);

   return o;
}

static Evas_Object *
_common_spinner_add(Evas_Object *parent,
                    Evas_Smart_Cb cb,
                    void *cb_data)
{
   Evas_Object *o;

   o = elm_spinner_add(parent);
   elm_spinner_editable_set(o, EINA_TRUE);
   elm_spinner_label_format_set(o, "%u");
   elm_spinner_step_set(o, 1.0);
   elm_spinner_wrap_set(o, EINA_FALSE);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_smart_callback_add(o, "changed", cb, cb_data);
   evas_object_show(o);

   return o;
}

static inline Pud_Unit_Description *
_pud_unit_ch_get(const Editor *ed)
{
   return &(ed->pud->units_descr[ed->menu_units->selected]);
}

static inline Pud_Upgrade_Description *
_pud_upgrade_ch_get(const Editor *ed)
{
   return &(ed->pud->upgrades[ed->menu_upgrades->selected]);
}

static void
_update_unit_sight(void        *data,
              Evas_Object *obj,
              void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->sight = val;
}

static Evas_Object *
_sight_widget(Evas_Object *parent,
              Editor      *ed,
              Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_min_max_set(o, 1.0, 9.0);

   return o;
}

static void
_update_unit_hp(void        *data,
                Evas_Object *obj,
                void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->hp = val;
}

static Evas_Object *
_hp_widget(Evas_Object *parent,
           Editor      *ed,
           Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_min_max_set(o, 1.0, (double)UINT16_MAX);

   return o;
}

static void
_update_unit_gold(void        *data,
                  Evas_Object *obj,
                  void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->gold_cost = val / 10.0;
}

static void
_update_unit_lumber(void        *data,
                    Evas_Object *obj,
                    void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->lumber_cost = val / 10.0;
}

static void
_update_unit_oil(void        *data,
                 Evas_Object *obj,
                 void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->oil_cost = val / 10.0;
}

static Evas_Object *
_gold_widget(Evas_Object *parent,
             Editor *ed,
             Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_step_set(o, 10.0);
   elm_spinner_min_max_set(o, 0.0, (double)UINT8_MAX * 10.0);

   return o;
}

static Evas_Object *
_lumber_widget(Evas_Object *parent,
               Editor *ed,
               Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_step_set(o, 10.0);
   elm_spinner_min_max_set(o, 0.0, (double)UINT8_MAX * 10.0);

   return o;
}

static Evas_Object *
_oil_widget(Evas_Object *parent,
            Editor *ed,
            Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_step_set(o, 10.0);
   elm_spinner_min_max_set(o, 0.0, (double)UINT8_MAX * 10.0);

   return o;
}

static void
_update_unit_range(void        *data,
                   Evas_Object *obj,
                   void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->range = val;
}

static Evas_Object *
_range_widget(Evas_Object *parent,
              Editor *ed,
              Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_min_max_set(o, 1.0, 9.0);

   return o;
}

static void
_update_unit_armor(void        *data,
                   Evas_Object *obj,
                   void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->armor = val;
}

static Evas_Object *
_armor_widget(Evas_Object *parent,
              Editor *ed,
              Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_min_max_set(o, 0.0, (double)UINT8_MAX);

   return o;
}

static void
_update_unit_basic(void        *data,
                   Evas_Object *obj,
                   void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->basic_damage = val;
}

static Evas_Object *
_basic_damage_widget(Evas_Object *parent,
                     Editor *ed,
                     Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_min_max_set(o, 0.0, (double)UINT8_MAX);

   return o;
}

static void
_update_unit_piercing(void        *data,
                      Evas_Object *obj,
                      void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->piercing_damage = val;
}

static Evas_Object *
_piercing_damage_wdiget(Evas_Object *parent,
                        Editor *ed,
                        Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_min_max_set(o, 0.0, (double)UINT8_MAX);

   return o;
}

static void
_missile_cb(void        *data,
            Evas_Object *obj,
            void        *info EINA_UNUSED)
{
   const Pud_Projectile p = (Pud_Projectile)((uintptr_t)data);
   Editor *ed;
   Pud_Unit_Description *c;

   ed = evas_object_data_get(obj, "editor");
   c = _pud_unit_ch_get(ed);
   c->missile_weapon = p;
}

static void
_update_units_time(void        *data,
                   Evas_Object *obj,
                   void        *info EINA_UNUSED)
{

   Editor *const ed = data;
   Pud_Unit_Description *ud;
   double val;

   ud = _pud_unit_ch_get(ed);
   val = elm_slider_value_get(obj);
   ud->build_time = val;
}

static Evas_Object *
_time_widget(Evas_Object *parent,
             Editor *ed,
             Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = elm_slider_add(parent);
   elm_slider_min_max_set(o, 0, 255);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(o, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_smart_callback_add(o, "changed", cb, ed);
   evas_object_show(o);

   return o;
}

static void
_free_data_cb(void        *data,
              Evas        *e    EINA_UNUSED,
              Evas_Object *obj  EINA_UNUSED,
              void        *info EINA_UNUSED)
{
   free(data);
}

Evas_Object *
menu_units_properties_new(Editor      *ed,
                          Evas_Object *parent)
{
   Evas_Object *f = NULL, *gen, *t, *b, *o;
   unsigned int i, n = 0;
   Menu_Units *mu;
   Elm_Genlist_Item_Class *itc;

   itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itc))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itc->item_style = "default";
   itc->func.text_get = _gen_units_text_get_cb;
   itc->func.content_get = _gen_units_icon_get_cb;

   mu = ed->menu_units = malloc(sizeof(*ed->menu_units));
   if (EINA_UNLIKELY(!ed->menu_units))
     {
        CRI("Failed to allocate memory");
        goto end;
     }

   f = _frame_add(parent, "Units Properties");
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   evas_object_event_callback_add(f, EVAS_CALLBACK_FREE, _free_data_cb, ed->menu_units);

   b = elm_box_add(f);
   evas_object_size_hint_weight_set(b, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(b, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(b, EINA_TRUE);
   elm_object_content_set(f, b);

   pud_default_udta_override(ed->pud, PUD_TRUE);

   gen = mu->gen = elm_genlist_add(b);
   evas_object_size_hint_weight_set(gen, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gen, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(gen, "editor", ed);
   elm_box_pack_start(b, gen);
   evas_object_show(gen);

   /*
    * All units but:
    * - start locations
    * - gold mine
    * - oil patch
    * - attack peon/peasant
    */
   for (i = 0; i <= PUD_UNIT_ORC_CANNON_TOWER; i++)
     if ((i != PUD_UNIT_GOLD_MINE) && (i != PUD_UNIT_OIL_PATCH) &&
         (i != PUD_UNIT_ATTACK_PEASANT) && (i != PUD_UNIT_ATTACK_PEON) &&
         (!pud_unit_start_location_is(i)) && (pud_unit_valid_is(i)))
       {
          elm_genlist_item_append(gen, itc, (Pud_Unit *)(uintptr_t)(i), NULL,
                                  ELM_GENLIST_ITEM_NONE, _unit_select_cb, ed);
       }


   t = _table_add(b);
   elm_table_homogeneous_set(t, EINA_FALSE);
   elm_box_pack_end(b, t);

   /* Settings column */
   mu->sight = _pack_ui(ed, t, n++, "Sight", _sight_widget, _update_unit_sight);
   mu->hp = _pack_ui(ed, t,  n++, "Hit Points", _hp_widget, _update_unit_hp);
   mu->gold = _pack_ui(ed, t,  n++, "Gold Cost", _gold_widget, _update_unit_gold);
   mu->lumber = _pack_ui(ed, t,  n++, "Lumber Cost", _lumber_widget, _update_unit_lumber);
   mu->oil = _pack_ui(ed, t,  n++, "Oil Cost", _oil_widget, _update_unit_oil);
   mu->range = _pack_ui(ed, t,  n++, "Range", _range_widget, _update_unit_range);
   mu->armor = _pack_ui(ed, t,  n++, "Armor", _armor_widget, _update_unit_armor);
   mu->basic_damage = _pack_ui(ed, t, n++, "Basic Damage", _basic_damage_widget, _update_unit_basic);
   mu->piercing_damage = _pack_ui(ed, t, n++, "Piercing Damage", _piercing_damage_wdiget, _update_unit_piercing);
   mu->time = _pack_ui(ed, t, n++, "Building Time", _time_widget, _update_units_time);
   mu->has_magic = _pack_ui_check(ed, t, n++, "Has Magic");
   mu->weapons_upgradable = _pack_ui_check(ed, t, n++, "Weapons Upgradable");
   mu->armor_upgradable = _pack_ui_check(ed, t, n++, "Armor Upgradable");

   /* Missile shapes editor */
   _pack_label_right(t, 0, n, "Projectile");
   mu->missile = o = _hoversel_add(t, pud_projectile_to_string(PUD_PROJECTILE_NONE));
   evas_object_data_set(o, "editor", ed);
   for (i = 0; i <= PUD_PROJECTILE_NONE; i++)
     elm_hoversel_item_add(o, pud_projectile_to_string(i), NULL, ELM_ICON_NONE,
                           _missile_cb, (void *)(uintptr_t)(i));
   elm_table_pack(t, o, 1, n++, 1, 1);

   /* Always make sure we have at least one selected element in the list */
   elm_genlist_item_selected_set(elm_genlist_first_item_get(gen), EINA_TRUE);

end:
   if (itc) elm_genlist_item_class_free(itc);
   return f;
}

/*============================================================================*
 *                             Upgrade Properties                             *
 *============================================================================*/

static void
_upgrade_select_cb(void        *data,
                   Evas_Object *obj  EINA_UNUSED,
                   void        *evt)
{
   /*
    *
    * Initial state of units
    *
    */

   Editor *const ed = data;
   const Pud_Upgrade u = (Pud_Upgrade)((uintptr_t)elm_object_item_data_get(evt));
   Menu_Upgrades *mu;
   Pud_Upgrade_Description *c;

   ed->menu_upgrades->selected = u;
   mu = ed->menu_upgrades;
   c = &(ed->pud->upgrades[u]);

   elm_spinner_value_set(mu->lumber, c->lumber);
   elm_spinner_value_set(mu->gold, c->gold);
   elm_spinner_value_set(mu->oil, c->oil);
   elm_slider_value_set(mu->time, c->time);
}

static void
_update_ugrd_gold(void        *data,
                  Evas_Object *obj,
                  void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Upgrade_Description *ud;
   double val;

   ud = _pud_upgrade_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->gold = val;
}

static void
_update_ugrd_lumber(void        *data,
                    Evas_Object *obj,
                    void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Upgrade_Description *ud;
   double val;

   ud = _pud_upgrade_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->lumber = val;
}

static void
_update_ugrd_oil(void        *data,
                 Evas_Object *obj,
                 void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Upgrade_Description *ud;
   double val;

   ud = _pud_upgrade_ch_get(ed);
   val = elm_spinner_value_get(obj);
   ud->oil = val;
}

static Evas_Object *
_gold_ugrd_widget(Evas_Object *parent,
                  Editor *ed,
                  Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_step_set(o, 10.0);
   elm_spinner_min_max_set(o, 0.0, (double)UINT16_MAX);

   return o;
}

static Evas_Object *
_lumber_ugrd_widget(Evas_Object *parent,
                    Editor *ed,
                    Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_step_set(o, 10.0);
   elm_spinner_min_max_set(o, 0.0, (double)UINT16_MAX);

   return o;
}

static Evas_Object *
_oil_ugrd_widget(Evas_Object *parent,
                 Editor *ed,
                 Evas_Smart_Cb cb)
{
   Evas_Object *o;

   o = _common_spinner_add(parent, cb, ed);
   elm_spinner_step_set(o, 10.0);
   elm_spinner_min_max_set(o, 0.0, (double)UINT16_MAX);

   return o;
}

static void
_update_ugrd_time(void        *data,
                  Evas_Object *obj,
                  void        *info EINA_UNUSED)
{

   Editor *const ed = data;
   Pud_Upgrade_Description *ud;
   double val;

   ud = _pud_upgrade_ch_get(ed);
   val = elm_slider_value_get(obj);
   ud->time = val;
}

Evas_Object *
menu_upgrades_properties_new(Editor *ed,
                             Evas_Object *parent)
{
   Evas_Object *f = NULL, *gen, *t, *b;
   unsigned int i, n = 0;
   Menu_Upgrades *mu;
   Elm_Genlist_Item_Class *itc;

   itc = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itc))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itc->item_style = "default";
   itc->func.text_get = _gen_upgrades_text_get_cb;
   itc->func.content_get = _gen_upgrades_icon_get_cb;

   mu = ed->menu_upgrades = malloc(sizeof(*ed->menu_upgrades));
   if (EINA_UNLIKELY(!ed->menu_upgrades))
     {
        CRI("Failed to allocate memory");
        goto end;
     }

   f = _frame_add(parent, "Upgrades Properties");
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   evas_object_event_callback_add(f, EVAS_CALLBACK_FREE, _free_data_cb, ed->menu_upgrades);

   b = elm_box_add(f);
   evas_object_size_hint_weight_set(b, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(b, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(b, EINA_TRUE);
   elm_object_content_set(f, b);

   pud_default_udta_override(ed->pud, PUD_TRUE);

   gen = mu->gen = elm_genlist_add(b);
   evas_object_size_hint_weight_set(gen, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gen, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(gen, "editor", ed);
   elm_box_pack_start(b, gen);
   evas_object_show(gen);

   for (i = 0; i < 52; i++)
     {
        elm_genlist_item_append(gen, itc, (Pud_Upgrade *)(uintptr_t)(i), NULL,
                                ELM_GENLIST_ITEM_NONE, _upgrade_select_cb, ed);
     }


   t = _table_add(b);
   elm_table_homogeneous_set(t, EINA_FALSE);
   elm_box_pack_end(b, t);

   /* Settings column */
   mu->time = _pack_ui(ed, t, n++, "Research Time", _time_widget, _update_ugrd_time);
   mu->gold = _pack_ui(ed, t,  n++, "Gold Cost", _gold_ugrd_widget, _update_ugrd_gold);
   mu->lumber = _pack_ui(ed, t,  n++, "Lumber Cost", _lumber_ugrd_widget, _update_ugrd_lumber);
   mu->oil = _pack_ui(ed, t,  n++, "Oil Cost", _oil_ugrd_widget, _update_ugrd_oil);

   /* Always make sure we have at least one selected element in the list */
   elm_genlist_item_selected_set(elm_genlist_first_item_get(gen), EINA_TRUE);

end:
   if (itc) elm_genlist_item_class_free(itc);
   return f;
}


/*============================================================================*
 *                              Allow Properties                              *
 *============================================================================*/

typedef enum
{
   ALLOW_GROUP_UNITS                = 0,
   ALLOW_GROUP_STARTING_SPELLS      = 1,
   ALLOW_GROUP_RESEARCH_SPELLS      = 2,
   ALLOW_GROUP_UPGRADES             = 3
} Allow_Group;

static char *
_player_text_get(void        *data,
                 Evas_Object *obj  EINA_UNUSED,
                 const char  *part EINA_UNUSED)
{
   const Pud_Player player = (Pud_Player)((uintptr_t)data);
   const char *const players_text[] = {
      STR_PLAYER_1,
      STR_PLAYER_2,
      STR_PLAYER_3,
      STR_PLAYER_4,
      STR_PLAYER_5,
      STR_PLAYER_6,
      STR_PLAYER_7,
      STR_PLAYER_8
   };

   return strdup(players_text[player]);
}

static char *
_group_text_get(void        *data,
                Evas_Object *obj  EINA_UNUSED,
                const char  *part EINA_UNUSED)
{
   const char *const str = data;
   return strdup(str);
}

static void
_alow_set(const Evas_Object *chk,
          Editor            *ed,
          Pud_Allow         *a)
{
   Eina_Bool on;

   on = elm_check_state_get(chk);
   if (on) *a |= ed->menu_allows->cur_allow;
   else *a &= ~(ed->menu_allows->cur_allow);
}


static void
_unit_alow_cb(void        *data,
              Evas_Object *obj,
              void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Allow *a;

   a = &(ed->pud->unit_alow.players[ed->menu_allows->cur_player]);
   _alow_set(obj, ed, a);
}

static void
_spell_start_cb(void        *data,
                Evas_Object *obj,
                void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Allow *a;

   a = &(ed->pud->spell_start.players[ed->menu_allows->cur_player]);
   _alow_set(obj, ed, a);
}

static void
_spell_search_cb(void        *data,
                 Evas_Object *obj,
                 void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Allow *a;

   a = &(ed->pud->spell_alow.players[ed->menu_allows->cur_player]);
   _alow_set(obj, ed, a);
}

static void
_upgrade_alow_cb(void        *data,
                 Evas_Object *obj,
                 void        *info EINA_UNUSED)
{
   Editor *const ed = data;
   Pud_Allow *a;

   a = &(ed->pud->up_alow.players[ed->menu_allows->cur_player]);
   _alow_set(obj, ed, a);
}

static inline Pud_Allow
_pud_allow_get(const void *data)
{
   return (const Pud_Allow)((const uintptr_t)data);
}

static Evas_Object *
_check_add(Evas_Object *parent,
           Evas_Smart_Cb cb,
           void *data)
{
   Evas_Object *o;

   o = elm_check_add(parent);
   evas_object_smart_callback_add(o, "changed", cb, data);
   //elm_object_style_set(o, "toggle");
   //elm_object_part_text_set(o, "on", "Yes");
   //elm_object_part_text_set(o, "off", "No");
   evas_object_show(o);

   return o;
}

static char *
_allow_text_get(void        *data,
                Evas_Object *obj  EINA_UNUSED,
                const char  *part EINA_UNUSED)
{
   const Pud_Allow alow = _pud_allow_get(data);
   if (!strcmp(part, "elm.text"))
     {
          return strdup(pud_allow_unit_to_string(alow));
     }
   return NULL;
}

static char *
_spell_start_text_get(void        *data,
                      Evas_Object *obj  EINA_UNUSED,
                      const char  *part EINA_UNUSED)
{
   const Pud_Allow alow = _pud_allow_get(data);
   if (!strcmp(part, "elm.text"))
     {
        return strdup(pud_allow_spell_to_string(alow));
     }
   return NULL;
}

static char *
_spell_search_text_get(void        *data,
                       Evas_Object *obj  EINA_UNUSED,
                       const char  *part EINA_UNUSED)
{
   const Pud_Allow alow = _pud_allow_get(data);
   if (!strcmp(part, "elm.text"))
     {
        return strdup(pud_allow_spell_to_string(alow));
     }
   return NULL;
}

static char *
_allow_upgrade_text_get(void        *data,
                        Evas_Object *obj  EINA_UNUSED,
                        const char  *part EINA_UNUSED)
{
   const Pud_Allow alow = _pud_allow_get(data);
   if (!strcmp(part, "elm.text"))
     {
        return strdup(pud_allow_upgrade_to_string(alow));
     }
   return NULL;
}

static Evas_Object *
_allow_content_get(void        *data,
                   Evas_Object *obj,
                   const char  *part)
{
   const Pud_Allow alow = _pud_allow_get(data);
   Evas_Object *im1, *im2, *box, *ret = NULL;
   Editor *const ed = evas_object_data_get(obj, "editor");
   const Pud_Icon *icons;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        ret = box = elm_box_add(obj);
        elm_box_horizontal_set(box, EINA_TRUE);
        elm_box_padding_set(box, 2, 0);

        icons = pud_allow_unit_icons_get(alow);
        im1 = editor_icon_image_new(box, icons[0], ed->pud->era, ed->menu_allows->cur_player);
        im2 = editor_icon_image_new(box, icons[1], ed->pud->era, ed->menu_allows->cur_player);
        elm_image_resizable_set(im1, EINA_FALSE, EINA_FALSE);
        elm_image_resizable_set(im2, EINA_FALSE, EINA_FALSE);

        elm_box_pack_start(box, im1);
        elm_box_pack_end(box, im2);
        evas_object_show(box);
     }
   else if (!strcmp(part, "elm.swallow.end"))
     {
         ret = _check_add(obj, _unit_alow_cb, ed);
         elm_check_state_set(
            ret,
            (ed->pud->unit_alow.players[ed->menu_allows->cur_player] & alow)
            ? EINA_TRUE : EINA_FALSE
         );
     }

   return ret;
}

static Evas_Object *
_spell_start_content_get(void        *data,
                         Evas_Object *obj,
                         const char  *part)
{
   const Pud_Allow alow = _pud_allow_get(data);
   Evas_Object *ret = NULL;
   Editor *const ed = evas_object_data_get(obj, "editor");
   Pud_Icon ic;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        ic = pud_allow_spell_icon_get(alow);
        ret = editor_icon_image_new(obj, ic, ed->pud->era, ed->menu_allows->cur_player);
        elm_image_resizable_set(ret, EINA_FALSE, EINA_FALSE);
     }
   else if (!strcmp(part, "elm.swallow.end"))
     {
        ret = _check_add(obj, _spell_start_cb, ed);
        elm_check_state_set(
           ret,
           (ed->pud->spell_start.players[ed->menu_allows->cur_player] & alow)
           ? EINA_TRUE : EINA_FALSE
        );
     }

   return ret;
}

static Evas_Object *
_spell_search_content_get(void        *data,
                          Evas_Object *obj,
                          const char  *part)
{
   const Pud_Allow alow = _pud_allow_get(data);
   Evas_Object *ret = NULL;
   Editor *const ed = evas_object_data_get(obj, "editor");
   Pud_Icon ic;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        ic = pud_allow_spell_icon_get(alow);
        ret = editor_icon_image_new(obj, ic, ed->pud->era, ed->menu_allows->cur_player);
        elm_image_resizable_set(ret, EINA_FALSE, EINA_FALSE);
     }
   else if (!strcmp(part, "elm.swallow.end"))
     {
        ret = _check_add(obj, _spell_search_cb, ed);
        elm_check_state_set(
           ret,
           (ed->pud->spell_alow.players[ed->menu_allows->cur_player] & alow)
           ? EINA_TRUE : EINA_FALSE
        );
     }

   return ret;
}

static Evas_Object *
_allow_upgrade_content_get(void        *data,
                           Evas_Object *obj,
                           const char  *part)
{
   const Pud_Allow alow = _pud_allow_get(data);
   Evas_Object *ret = NULL, *box, *im1, *im2;
   Editor *const ed = evas_object_data_get(obj, "editor");
   const Pud_Icon *icons;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        ret = box = elm_box_add(obj);
        elm_box_horizontal_set(box, EINA_TRUE);
        elm_box_padding_set(box, 2, 0);

        icons = pud_allow_upgrade_icons_get(alow);
        im1 = editor_icon_image_new(box, icons[0], ed->pud->era, ed->menu_allows->cur_player);
        im2 = editor_icon_image_new(box, icons[1], ed->pud->era, ed->menu_allows->cur_player);
        elm_image_resizable_set(im1, EINA_FALSE, EINA_FALSE);
        elm_image_resizable_set(im2, EINA_FALSE, EINA_FALSE);

        elm_box_pack_start(box, im1);
        elm_box_pack_end(box, im2);
        evas_object_show(box);
     }
   else if (!strcmp(part, "elm.swallow.end"))
     {
        ret = _check_add(obj, _upgrade_alow_cb, ed);
        elm_check_state_set(
           ret,
           (ed->pud->up_alow.players[ed->menu_allows->cur_player] & alow)
           ? EINA_TRUE : EINA_FALSE
        );
     }

   return ret;
}

static void
_select_player_cb(void        *data,
                  Evas_Object *obj  EINA_UNUSED,
                  void        *evt)
{
   Editor *const ed = data;
   const Pud_Player p = (Pud_Player)((uintptr_t)elm_object_item_data_get(evt));

   ed->menu_allows->cur_player = p;
   elm_genlist_realized_items_update(ed->menu_allows->gen);
}

static void
_select_alow_cb(void        *data,
                Evas_Object *obj  EINA_UNUSED,
                void        *evt)
{
   Editor *const ed = data;
   const Pud_Allow a = (Pud_Allow)((uintptr_t)elm_object_item_data_get(evt));

   ed->menu_allows->cur_allow = a;
}


Evas_Object *
menu_allow_properties_new(Editor *ed,
                          Evas_Object *parent)
{
   Evas_Object *f = NULL, *gen, *gen2, *b;
   unsigned int i;
   Elm_Genlist_Item_Class *itcp, *itcg = NULL, *itca = NULL, *itcss = NULL, *itcsr = NULL,  *itcu = NULL;
   Elm_Object_Item *groups[4], *eoi;
   Pud_Allow fl;

   /* Players ITC */
   itcp = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itcp))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itcp->item_style = "default";
   itcp->func.text_get = _player_text_get;

   /* Group IRC */
   itcg = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itcg))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itcg->item_style = "group_index";
   itcg->func.text_get = _group_text_get;

   /* Allowed units ITC */
   itca = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itca))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itca->item_style = "double_label";
   itca->func.text_get = _allow_text_get;
   itca->func.content_get = _allow_content_get;

   /* Starting spells ITC */
   itcss = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itcss))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itcss->item_style = "double_label";
   itcss->func.text_get = _spell_start_text_get;
   itcss->func.content_get = _spell_start_content_get;

   /* Research spells ITC */
   itcsr = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itcsr))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itcsr->item_style = "double_label";
   itcsr->func.text_get = _spell_search_text_get;
   itcsr->func.content_get = _spell_search_content_get;

   /* Allowed Upgrades ITC */
   itcu = elm_genlist_item_class_new();
   if (EINA_UNLIKELY(!itcu))
     {
        CRI("Failed to create genlist item class");
        goto end;
     }
   itcu->item_style = "double_label";
   itcu->func.text_get = _allow_upgrade_text_get;
   itcu->func.content_get = _allow_upgrade_content_get;

   ed->menu_allows = calloc(1, sizeof(*ed->menu_allows));
   if (EINA_UNLIKELY(!ed->menu_allows))
     {
        CRI("Failed to allocate memory");
        goto end;
     }

   f = _frame_add(parent, "Allow Properties");
   evas_object_size_hint_weight_set(f, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   evas_object_event_callback_add(f, EVAS_CALLBACK_FREE, _free_data_cb, ed->menu_allows);

   b = elm_box_add(f);
   evas_object_size_hint_weight_set(b, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(b, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(b, EINA_TRUE);
   elm_object_content_set(f, b);

   /*
    * PUD files are not expected to see their ALOW section manipulated,
    * because Blizzard did not provide support for this in their editor
    * back then. Therefore, the default ALOW section is "empty": nothing
    * will be allowed. Our defaults are the opposite: eveything is allowed.
    * It is necessary to use our defaults, otherwise the user would have to
    * manually enable everything.
    */
   if (pud_default_alow_get(ed->pud))
     {
        pud_alow_defaults_set(ed->pud);
     }
   pud_default_alow_override(ed->pud, PUD_TRUE);

   gen = elm_genlist_add(b);
   evas_object_size_hint_weight_set(gen, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gen, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(gen, "editor", ed);
   elm_box_pack_start(b, gen);
   evas_object_show(gen);

   for (i = 0; i <= 7; i++)
     {
        elm_genlist_item_append(gen, itcp, (Pud_Player *)((uintptr_t)i), NULL,
                                ELM_GENLIST_ITEM_NONE, _select_player_cb, ed);
     }

   gen2 = ed->menu_allows->gen = elm_genlist_add(b);
   evas_object_size_hint_weight_set(gen2, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(gen2, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(gen2, "editor", ed);
   elm_box_pack_end(b, gen2);
   evas_object_show(gen2);

   /* Allowed Units & Buildings */
   eoi = groups[ALLOW_GROUP_UNITS] = elm_genlist_item_append(
      gen2, itcg, "Units and Buildings Allowed", NULL,
      ELM_GENLIST_ITEM_GROUP, NULL, NULL
   );
   for (i = 0; i < sizeof(Pud_Allow) * 8; i++)
     {
        fl = (1 << i);
        if (pud_allow_unit_valid_is(fl))
          {
             elm_genlist_item_append(gen2, itca, (Pud_Allow *)((uintptr_t)fl), eoi,
                                     ELM_GENLIST_ITEM_NONE, _select_alow_cb, ed);
          }
     }

   /* Starting Spells */
   eoi = groups[ALLOW_GROUP_STARTING_SPELLS] = elm_genlist_item_append(
      gen2, itcg, "Starting Spells", NULL,
      ELM_GENLIST_ITEM_GROUP, NULL, NULL
   );
   for (i = 0; i < sizeof(Pud_Allow) * 8; i++)
     {
        fl = (1 << i);
        if (pud_allow_spell_valid_is(fl))
          {
             elm_genlist_item_append(gen2, itcss, (Pud_Allow *)((uintptr_t)fl), eoi,
                                     ELM_GENLIST_ITEM_NONE, _select_alow_cb, ed);
          }
     }

   /* Research Spells */
   eoi = groups[ALLOW_GROUP_RESEARCH_SPELLS] = elm_genlist_item_append(
      gen2, itcg, "Spells Allowed to Research", NULL,
      ELM_GENLIST_ITEM_GROUP, NULL, NULL
   );
   for (i = 0; i < sizeof(Pud_Allow) * 8; i++)
     {
        fl = (1 << i);
        if (pud_allow_spell_valid_is(fl))
          {
             elm_genlist_item_append(gen2, itcsr, (Pud_Allow *)((uintptr_t)fl), eoi,
                                     ELM_GENLIST_ITEM_NONE, _select_alow_cb, ed);
          }
     }

   eoi = groups[ALLOW_GROUP_UPGRADES] = elm_genlist_item_append(
      gen2, itcg, "Upgrades Allowed to Reseach", NULL,
      ELM_GENLIST_ITEM_GROUP, NULL, NULL
   );
   for (i = 0; i < sizeof(Pud_Allow) * 8; i++)
     {
        fl = (1 << i);
        if (pud_allow_upgrade_valid_is(fl))
          {
             elm_genlist_item_append(gen2, itcu, (Pud_Allow *)((uintptr_t)fl), eoi,
                                     ELM_GENLIST_ITEM_NONE, _select_alow_cb, ed);
          }
     }

   /* Always make sure we have at least one selected element in the list */
   elm_genlist_item_selected_set(elm_genlist_first_item_get(gen), EINA_TRUE);

end:
   if (itcp) elm_genlist_item_class_free(itcp);
   if (itcg) elm_genlist_item_class_free(itcg);
   if (itca) elm_genlist_item_class_free(itca);
   if (itcu) elm_genlist_item_class_free(itcu);
   if (itcss) elm_genlist_item_class_free(itcss);
   if (itcsr) elm_genlist_item_class_free(itcsr);
   return f;
}
