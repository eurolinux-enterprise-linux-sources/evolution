/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_SEARCHING_TOKENIZER_H__
#define __E_SEARCHING_TOKENIZER_H__

#include <glib.h>
#include <gtkhtml/htmltokenizer.h>

#define E_TYPE_SEARCHING_TOKENIZER        (e_searching_tokenizer_get_type ())
#define E_SEARCHING_TOKENIZER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_SEARCHING_TOKENIZER, ESearchingTokenizer))
#define E_SEARCHING_TOKENIZER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_SEARCHING_TOKENIZER, ESearchingTokenizerClass))
#define E_IS_SEARCHING_TOKENIZER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_SEARCHING_TOKENIZER))
#define E_IS_SEARCHING_TOKENIZER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_TYPE_SEARCHING_TOKENIZER))

typedef struct _ESearchingTokenizer ESearchingTokenizer;
typedef struct _ESearchingTokenizerClass ESearchingTokenizerClass;

struct _ESearchingTokenizerPrivate;

struct _ESearchingTokenizer {
	HTMLTokenizer parent;

	struct _ESearchingTokenizerPrivate *priv;
};

struct _ESearchingTokenizerClass {
	HTMLTokenizerClass parent_class;

	void (*match) (ESearchingTokenizer *);
};

GType e_searching_tokenizer_get_type (void);

ESearchingTokenizer *e_searching_tokenizer_new (void);

/* For now, just a simple API */

void e_searching_tokenizer_set_primary_search_string    (ESearchingTokenizer *, const gchar *);
void e_searching_tokenizer_add_primary_search_string    (ESearchingTokenizer *, const gchar *);
void e_searching_tokenizer_set_primary_case_sensitivity (ESearchingTokenizer *, gboolean is_case_sensitive);

void e_searching_tokenizer_set_secondary_search_string    (ESearchingTokenizer *, const gchar *);
void e_searching_tokenizer_add_secondary_search_string (ESearchingTokenizer *st, const gchar *search_str);
void e_searching_tokenizer_set_secondary_case_sensitivity (ESearchingTokenizer *, gboolean is_case_sensitive);

gint e_searching_tokenizer_match_count          (ESearchingTokenizer *);

#endif /* __E_SEARCHING_TOKENIZER_H__ */
