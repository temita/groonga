/* -*- c-basic-offset: 2 -*- */
/* Copyright(C) 2009 Brazil

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
#ifndef GRN_PROC_H
#define GRN_PROC_H

#ifndef GROONGA_IN_H
#include "groonga_in.h"
#endif /* GROONGA_IN_H */

#ifdef __cplusplus
extern "C" {
#endif

GRN_VAR const char *grn_document_root;
void grn_db_init_builtin_query(grn_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* GRN_PROC_H */
