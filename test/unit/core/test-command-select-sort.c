/* -*- c-basic-offset: 2; coding: utf-8 -*- */
/*
  Copyright (C) 2010-2011  Kouhei Sutou <kou@clear-code.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "str.h"
#include <stdio.h>

#include <gcutter.h>

#include "../lib/grn-assertions.h"

void test_int(void);
void test_drilldown(void);
void test_score_without_query(void);
void test_score_drilldown_without_query(void);
void test_nonexistent(void);
void test_index(void);

static gchar *tmp_directory;

static grn_ctx *context;
static grn_obj *database;

void
cut_startup(void)
{
  tmp_directory = g_build_filename(grn_test_get_tmp_dir(),
                                   "command-select-sort",
                                   NULL);
}

void
cut_shutdown(void)
{
  g_free(tmp_directory);
}

static void
remove_tmp_directory(void)
{
  cut_remove_path(tmp_directory, NULL);
}

static void
setup_ddl(void)
{
  assert_send_commands("table_create Sites TABLE_PAT_KEY ShortText\n"
                       "column_create Sites score COLUMN_SCALAR Int32\n"
                       "column_create Sites age COLUMN_SCALAR Int32\n"
                       "column_create Sites description COLUMN_SCALAR ShortText");

  assert_send_commands("table_create Users TABLE_PAT_KEY ShortText\n"
                       "column_create Users name COLUMN_SCALAR ShortText");

  assert_send_commands("table_create Bookmarks TABLE_NO_KEY\n"
                       "column_create Bookmarks site COLUMN_SCALAR Sites\n"
                       "column_create Bookmarks user COLUMN_SCALAR Users\n"
                       "column_create Bookmarks rank COLUMN_SCALAR Int32");

  assert_send_commands("column_create Users bookmarks COLUMN_VECTOR Bookmarks "
                       "--source Bookmarks.user");

  assert_send_commands("table_create Bigram TABLE_PAT_KEY ShortText "
                       "--default_tokenizer TokenBigram\n"
                       "column_create Bigram description "
                       "COLUMN_INDEX|WITH_POSITION Sites description");

}

static void
setup_data(void)
{
  assert_send_commands("load --table Sites\n"
                       "[\n"
                       "[\"_key\", \"score\", \"age\", \"description\"],\n"
                       "[\"groonga.org\", 100, 2, "
                       "\"fulltext search engine and column store\"],\n"
                       "[\"qwik.jp/senna/FrontPageJ.html\", 100, 5, "
                       "\"embeddable fulltext search engine\"],\n"
                       "[\"2ch.net\", 10, 11, \"BBS\"]\n"
                       "]");

  assert_send_commands("load --table Users\n"
                       "[\n"
                       "[\"_key\", \"name\"],\n"
                       "[\"morita\", \"Daijiro MORI\"],\n"
                       "[\"gunyara-kun\", \"Tasuku SUENAGA\"],\n"
                       "[\"yu\", \"Yutaro Shimamura\"]\n"
                       "]");

  assert_send_commands("load --table Bookmarks\n"
                       "[\n"
                       "[\"site\", \"user\", \"rank\"],\n"
                       "[\"groonga.org\", \"morita\", 100],\n"
                       "[\"groonga.org\", \"gunyara-kun\", 100],\n"
                       "[\"groonga.org\", \"yu\", 50],\n"
                       "[\"2ch.net\", \"gunyara-kun\", null],\n"
                       "[\"2ch.net\", \"yu\", 10]\n"
                       "]");
}

void
cut_setup(void)
{
  const gchar *database_path;

  remove_tmp_directory();
  g_mkdir_with_parents(tmp_directory, 0700);

  context = g_new0(grn_ctx, 1);
  grn_ctx_init(context, 0);

  database_path = cut_build_path(tmp_directory, "database.groonga", NULL);
  database = grn_db_create(context, database_path, NULL);

  setup_ddl();
  setup_data();
}

void
cut_teardown(void)
{
  if (context) {
    grn_obj_unlink(context, database);
    grn_ctx_fin(context);
    g_free(context);
  }

  remove_tmp_directory();
}

void
test_int(void)
{
  cut_assert_equal_string(
    "[[[3],"
     "[[\"_key\",\"ShortText\"],"
      "[\"score\",\"Int32\"],"
      "[\"age\",\"Int32\"]],"
     "[\"qwik.jp/senna/FrontPageJ.html\",100,5],"
     "[\"groonga.org\",100,2],"
     "[\"2ch.net\",10,11]]]",
    send_command("select Sites "
                 "--sortby \"-score, -age\" "
                 "--output_columns \"_key, score, age\""));
}

void
test_drilldown(void)
{
  cut_assert_equal_string(
    "[[[5],"
     "[[\"site._key\",\"ShortText\"],"
      "[\"user._key\",\"ShortText\"],"
      "[\"rank\",\"Int32\"]],"
     "[\"groonga.org\",\"morita\",100],"
     "[\"groonga.org\",\"gunyara-kun\",100],"
     "[\"groonga.org\",\"yu\",50],"
     "[\"2ch.net\",\"gunyara-kun\",0],"
     "[\"2ch.net\",\"yu\",10]],"
     "[[2],"
      "[[\"_key\",\"ShortText\"],"
       "[\"_nsubrecs\",\"Int32\"]],"
      "[\"2ch.net\",2],"
      "[\"groonga.org\",3]],"
     "[[3],"
      "[[\"_key\",\"ShortText\"],"
       "[\"_nsubrecs\",\"Int32\"]],"
      "[\"gunyara-kun\",2],"
      "[\"morita\",1],"
      "[\"yu\",2]],"
     "[[4],"
       "[[\"_key\",\"Int32\"],"
        "[\"_nsubrecs\",\"Int32\"]],"
      "[0,1],"
      "[10,1],"
      "[50,1],"
      "[100,2]]]",
    send_command("select Bookmarks "
                 "--output_columns \"site._key, user._key, rank\" "
                 "--sortby \"-site.score, -site.age\" "
                 "--drilldown \"site, user, rank\" "
                 "--drilldown_output_columns \"_key, _nsubrecs\" "
                 "--drilldown_sortby \"_key\""));
}

void
test_score_without_query(void)
{
  cut_assert_equal_string(
    "[[[3],"
      "[[\"_key\",\"ShortText\"]],"
      "[\"2ch.net\"],"
      "[\"groonga.org\"],"
      "[\"qwik.jp/senna/FrontPageJ.html\"]]]",
    send_command("select Sites "
                 "--sortby \"_score\" "
                 "--output_columns \"_key\""));
}

void
test_score_drilldown_without_query(void)
{
  cut_assert_equal_string(
    "[[[5],"
      "[[\"_id\",\"UInt32\"],"
       "[\"site._key\",\"ShortText\"],"
       "[\"user._key\",\"ShortText\"]],"
      "[5,\"2ch.net\",\"yu\"],"
      "[4,\"2ch.net\",\"gunyara-kun\"],"
      "[3,\"groonga.org\",\"yu\"],"
      "[2,\"groonga.org\",\"gunyara-kun\"],"
      "[1,\"groonga.org\",\"morita\"]],"
     "[[2],"
      "[[\"_key\",\"ShortText\"],"
       "[\"_nsubrecs\",\"Int32\"]],"
      "[\"2ch.net\",2],"
      "[\"groonga.org\",3]],"
     "[[3],"
      "[[\"_key\",\"ShortText\"],"
       "[\"_nsubrecs\",\"Int32\"]],"
      "[\"gunyara-kun\",2],"
      "[\"morita\",1],"
      "[\"yu\",2]],"
    "[[4],"
     "[[\"_key\",\"Int32\"],"
      "[\"_nsubrecs\",\"Int32\"]],"
     "[0,1],"
     "[10,1],"
     "[50,1],"
     "[100,2]]]",
    send_command("select Bookmarks "
                 "--sortby \"_score,-_id\" "
                 "--output_columns \"_id, site._key, user._key\" "
                 "--drilldown \"site user rank\" "
                 "--drilldown_output_columns \"_key, _nsubrecs\" "
                 "--drilldown_sortby \"_key\""));
}

void
test_nonexistent(void)
{
  grn_test_assert_send_command_error(
    context,
    GRN_INVALID_ARGUMENT,
    "invalid sort key: <nonexistent>(<_score,nonexistent>)",
    "select Sites "
    "--sortby \"_score,nonexistent\" "
    "--output_columns \"_key\"");
}

void
test_index(void)
{
  cut_assert_equal_string(
    "[[[2],"
      "[[\"_key\",\"ShortText\"],[\"description\",\"ShortText\"]],"
       "[\"qwik.jp/senna/FrontPageJ.html\","
        "\"embeddable fulltext search engine\"],"
       "[\"groonga.org\","
        "\"fulltext search engine and column store\"]]]",
    send_command("select Sites "
                 "--sortby \"description\" "
                 "--output_columns \"_key, description\" "
                 "--match_columns \"description\" "
                 "--query \"fulltext\""));
}
