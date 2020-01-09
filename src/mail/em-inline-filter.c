/*
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "em-inline-filter.h"
#include <camel/camel-mime-part.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>

#include "em-utils.h"

#define d(x)

static void em_inline_filter_class_init (EMInlineFilterClass *klass);
static void em_inline_filter_init (CamelObject *object);
static void em_inline_filter_finalize (CamelObject *object);

static void emif_filter(CamelMimeFilter *f, const gchar *in, gsize len, gsize prespace, gchar **out, gsize *outlen, gsize *outprespace);
static void emif_complete(CamelMimeFilter *f, const gchar *in, gsize len, gsize prespace, gchar **out, gsize *outlen, gsize *outprespace);
static void emif_reset(CamelMimeFilter *f);

static CamelMimeFilterClass *parent_class = NULL;

CamelType
em_inline_filter_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;

	if (type == CAMEL_INVALID_TYPE) {
		parent_class = (CamelMimeFilterClass *)camel_mime_filter_get_type();

		type = camel_type_register(camel_mime_filter_get_type(),
					   "EMInlineFilter",
					   sizeof (EMInlineFilter),
					   sizeof (EMInlineFilterClass),
					   (CamelObjectClassInitFunc) em_inline_filter_class_init,
					   NULL,
					   (CamelObjectInitFunc) em_inline_filter_init,
					   (CamelObjectFinalizeFunc) em_inline_filter_finalize);
	}

	return type;
}

static void
em_inline_filter_class_init (EMInlineFilterClass *klass)
{
	((CamelMimeFilterClass *)klass)->filter = emif_filter;
	((CamelMimeFilterClass *)klass)->complete = emif_complete;
	((CamelMimeFilterClass *)klass)->reset = emif_reset;
}

static void
em_inline_filter_init (CamelObject *object)
{
	EMInlineFilter *emif = (EMInlineFilter *)object;

	emif->data = g_byte_array_new();
}

static void
em_inline_filter_finalize (CamelObject *object)
{
	EMInlineFilter *emif = (EMInlineFilter *)object;

	if (emif->base_type)
		camel_content_type_unref(emif->base_type);

	emif_reset((CamelMimeFilter *)emif);
	g_byte_array_free(emif->data, TRUE);
	g_free(emif->filename);
}

enum {
	EMIF_PLAIN,
	EMIF_UUENC,
	EMIF_BINHEX,
	EMIF_POSTSCRIPT,
	EMIF_PGPSIGNED,
	EMIF_PGPENCRYPTED
};

static const struct {
	const gchar *type;
	const gchar *subtype;
	CamelTransferEncoding encoding;
	guint plain:1;
} emif_types[] = {
	{ "text",        "plain",                 CAMEL_TRANSFER_ENCODING_DEFAULT,  1, },
	{ "application", "octet-stream",          CAMEL_TRANSFER_ENCODING_UUENCODE, 0, },
	{ "application", "mac-binhex40",          CAMEL_TRANSFER_ENCODING_7BIT,     0, },
	{ "application", "postscript",            CAMEL_TRANSFER_ENCODING_7BIT,     0, },
	{ "application", "x-inlinepgp-signed",    CAMEL_TRANSFER_ENCODING_DEFAULT,  0, },
	{ "application", "x-inlinepgp-encrypted", CAMEL_TRANSFER_ENCODING_DEFAULT,  0, },
};

static void
emif_add_part(EMInlineFilter *emif, const gchar *data, gint len)
{
	CamelTransferEncoding encoding;
	CamelContentType *content_type;
	CamelDataWrapper *dw;
	const gchar *mimetype;
	CamelMimePart *part;
	CamelStream *mem;
	gchar *type;

	if (emif->state == EMIF_PLAIN || emif->state == EMIF_PGPSIGNED || emif->state == EMIF_PGPENCRYPTED)
		encoding = emif->base_encoding;
	else
		encoding = emif_types[emif->state].encoding;

	g_byte_array_append(emif->data, (guchar *)data, len);
	/* check the part will actually have content */
	if (emif->data->len <= 0) {
		return;
	}
	mem = camel_stream_mem_new_with_byte_array(emif->data);
	emif->data = g_byte_array_new();

	dw = camel_data_wrapper_new();
	camel_data_wrapper_construct_from_stream(dw, mem);
	camel_object_unref(mem);

	if (emif_types[emif->state].plain && emif->base_type) {
		camel_content_type_ref (emif->base_type);
		content_type = emif->base_type;
	} else {
		/* we want to preserve all params */
		type = camel_content_type_format (emif->base_type);
		content_type = camel_content_type_decode (type);
		g_free (type);

		g_free (content_type->type);
		g_free (content_type->subtype);
		content_type->type = g_strdup (emif_types[emif->state].type);
		content_type->subtype = g_strdup (emif_types[emif->state].subtype);
	}

	camel_data_wrapper_set_mime_type_field (dw, content_type);
	camel_content_type_unref (content_type);
	dw->encoding = encoding;

	part = camel_mime_part_new();
	camel_medium_set_content_object((CamelMedium *)part, dw);
	camel_mime_part_set_encoding(part, encoding);
	camel_object_unref(dw);

	if (emif->filename)
		camel_mime_part_set_filename(part, emif->filename);

	/* pre-snoop the mime type of unknown objects, and poke and hack it into place */
	if (camel_content_type_is(dw->mime_type, "application", "octet-stream")
	    && (mimetype = em_utils_snoop_type(part))
	    && strcmp(mimetype, "application/octet-stream") != 0) {
		camel_data_wrapper_set_mime_type(dw, mimetype);
		camel_mime_part_set_content_type(part, mimetype);
		if (emif->filename)
			camel_mime_part_set_filename(part, emif->filename);
	}

	g_free(emif->filename);
	emif->filename = NULL;

	emif->parts = g_slist_append(emif->parts, part);
}

static gint
emif_scan(CamelMimeFilter *f, gchar *in, gsize len, gint final)
{
	EMInlineFilter *emif = (EMInlineFilter *)f;
	gchar *inptr = in, *inend = in+len;
	gchar *data_start = in;
	gchar *start = in;

	while (inptr < inend) {
		start = inptr;

		while (inptr < inend && *inptr != '\n')
			inptr++;

		if (inptr == inend) {
			if (!final) {
				camel_mime_filter_backup(f, start, inend-start);
				inend = start;
			}
			break;
		}

		*inptr++ = 0;

		switch (emif->state) {
		case EMIF_PLAIN:
			/* This could use some funky plugin shit, but this'll do for now */
			if (strncmp(start, "begin ", 6) == 0
			    && start[6] >= '0' && start[6] <= '7') {
				gint i = 7;
				gchar *name;

				while (start[i] >='0' && start[i] <='7')
					i++;

				inptr[-1] = '\n';

				if (start[i++] != ' ')
					break;

				emif_add_part(emif, data_start, start-data_start);

				name = g_strndup(start+i, inptr-start-i-1);
				emif->filename = camel_header_decode_string(name, emif->base_type?camel_content_type_param(emif->base_type, "charset"):NULL);
				g_free(name);
				data_start = start;
				emif->state = EMIF_UUENC;
			} else if (strncmp(start, "(This file must be converted with BinHex 4.0)", 45) == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_BINHEX;
			} else if (strncmp(start, "%!PS-Adobe-", 11) == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_POSTSCRIPT;
			} else if (strncmp(start, "-----BEGIN PGP SIGNED MESSAGE-----", 34) == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_PGPSIGNED;
			} else if (strncmp(start, "-----BEGIN PGP MESSAGE-----", 27) == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, start-data_start);
				data_start = start;
				emif->state = EMIF_PGPENCRYPTED;
			}

			break;
		case EMIF_UUENC:
			if (strcmp(start, "end") == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			} else {
				gint linelen;

				/* check the length byte matches the data, if not, output what we have and re-scan this line */
				len = ((start[0] - ' ') & 077);
				linelen = inptr-start-1;
				while (linelen > 0 && (start[linelen] == '\r' || start[linelen] == '\n'))
					linelen--;
				linelen--;
				linelen /= 4;
				linelen *= 3;
				if (!(len == linelen || len == linelen-1 || len == linelen-2)) {
					inptr[-1] = '\n';
					emif_add_part(emif, data_start, start-data_start);
					data_start = start;
					inptr = start;
					emif->state = EMIF_PLAIN;
					continue;
				}
			}
			break;
		case EMIF_BINHEX:
			if (inptr > (start+1) && inptr[-2] == ':') {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_POSTSCRIPT:
			if (strcmp(start, "%%EOF") == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_PGPSIGNED:
			if (strcmp(start, "-----END PGP SIGNATURE-----") == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		case EMIF_PGPENCRYPTED:
			if (strcmp(start, "-----END PGP MESSAGE-----") == 0) {
				inptr[-1] = '\n';
				emif_add_part(emif, data_start, inptr-data_start);
				data_start = inptr;
				emif->state = EMIF_PLAIN;
			}
			break;
		}

		inptr[-1] = '\n';
	}

	if (final) {
		/* always stop as plain, especially when not read those tags fully */
		emif->state = EMIF_PLAIN;

		emif_add_part(emif, data_start, inend-data_start);
	} else {
		g_byte_array_append(emif->data, (guchar *)data_start, inend-data_start);
	}

	return 0;
}

static void
emif_filter(CamelMimeFilter *f, const gchar *in, gsize len, gsize prespace, gchar **out, gsize *outlen, gsize *outprespace)
{
	emif_scan(f, (gchar *)in, len, FALSE);

	*out = (gchar *)in;
	*outlen = len;
	*outprespace = prespace;
}

static void
emif_complete(CamelMimeFilter *f, const gchar *in, gsize len, gsize prespace, gchar **out, gsize *outlen, gsize *outprespace)
{
	emif_scan(f, (gchar *)in, len, TRUE);

	*out = (gchar *)in;
	*outlen = len;
	*outprespace = prespace;
}

static void
emif_reset(CamelMimeFilter *f)
{
	EMInlineFilter *emif = (EMInlineFilter *)f;
	GSList *l;

	l = emif->parts;
	while (l) {
		GSList *n = l->next;

		camel_object_unref(l->data);
		g_slist_free_1(l);

		l = n;
	}
	emif->parts = NULL;
	g_byte_array_set_size(emif->data, 0);
}

/**
 * em_inline_filter_new:
 * @base_encoding: The base transfer-encoding of the
 * raw data being processed.
 * @base_type: The base content-type of the raw data, should always be
 * text/plain.
 *
 * Create a filter which will scan a (text) stream for
 * embedded parts.  You can then retrieve the contents
 * as a CamelMultipart object.
 *
 * Return value:
 **/
EMInlineFilter *
em_inline_filter_new(CamelTransferEncoding base_encoding, CamelContentType *base_type)
{
	EMInlineFilter *emif;

	emif = (EMInlineFilter *)camel_object_new(em_inline_filter_get_type());
	emif->base_encoding = base_encoding;
	if (base_type) {
		emif->base_type = base_type;
		camel_content_type_ref(emif->base_type);
	}

	return emif;
}

CamelMultipart *
em_inline_filter_get_multipart(EMInlineFilter *emif)
{
	GSList *l = emif->parts;
	CamelMultipart *mp;

	mp = camel_multipart_new();
	while (l) {
		camel_multipart_add_part(mp, l->data);
		l = l->next;
	}

	return mp;
}
