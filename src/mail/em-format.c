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
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libedataserver/e-msgport.h>
#include <camel/camel-url.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-multipart.h>
#include <camel/camel-multipart-encrypted.h>
#include <camel/camel-multipart-signed.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-gpg-context.h>
#include <camel/camel-smime-context.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-mime-filter-windows.h>
#include <camel/camel-mime-filter-pgp.h>

#include "em-format.h"
#include "em-utils.h"
#include "mail-config.h"

#define d(x)

/* Used to cache various data/info for redraws
   The validity stuff could be cached at a higher level but this is easier
   This absolutely relies on the partid being _globally unique_
   This is still kind of yucky, we should maintian a full tree of all this data,
   along with/as part of the puri tree */
struct _EMFormatCache {
	CamelCipherValidity *valid; /* validity copy */
	CamelMimePart *secured;	/* encrypted subpart */

	guint state:2;		/* inline state */

	gchar partid[1];
};

#define INLINE_UNSET (0)
#define INLINE_ON (1)
#define INLINE_OFF (2)

static void emf_builtin_init(EMFormatClass *);

static const EMFormatHandler *emf_find_handler(EMFormat *emf, const gchar *mime_type);
static void emf_format_clone(EMFormat *emf, CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, EMFormat *emfsource);
static void emf_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid);
static gboolean emf_busy(EMFormat *emf);
enum {
	EMF_COMPLETE,
	EMF_LAST_SIGNAL
};

extern CamelSession *session;

static guint emf_signals[EMF_LAST_SIGNAL];
static GObjectClass *emf_parent;

static void
emf_free_cache(struct _EMFormatCache *efc)
{
	if (efc->valid)
		camel_cipher_validity_free(efc->valid);
	if (efc->secured)
		camel_object_unref(efc->secured);
	g_free(efc);
}

static struct _EMFormatCache *
emf_insert_cache(EMFormat *emf, const gchar *partid)
{
	struct _EMFormatCache *new;

	new = g_malloc0(sizeof(*new)+strlen(partid));
	strcpy(new->partid, partid);
	g_hash_table_insert(emf->inline_table, new->partid, new);

	return new;
}

static void
emf_init(GObject *o)
{
	EMFormat *emf = (EMFormat *)o;

	emf->inline_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) emf_free_cache);
	emf->composer = FALSE;
	emf->print = FALSE;
	emf->show_photo = TRUE;
	emf->photo_local = TRUE;
	e_dlist_init(&emf->header_list);
	em_format_default_headers(emf);
	emf->part_id = g_string_new("");
}

static void
emf_finalise(GObject *o)
{
	EMFormat *emf = (EMFormat *)o;

	if (emf->session)
		camel_object_unref(emf->session);

	g_hash_table_destroy(emf->inline_table);

	em_format_clear_headers(emf);
	camel_cipher_validity_free(emf->valid);
	g_free(emf->charset);
	g_free (emf->default_charset);
	g_string_free(emf->part_id, TRUE);

	/* FIXME: check pending jobs */

	((GObjectClass *)emf_parent)->finalize(o);
}

static void
emf_base_init(EMFormatClass *emfklass)
{
	emfklass->type_handlers = g_hash_table_new(g_str_hash, g_str_equal);
	emf_builtin_init(emfklass);
}

static void
emf_class_init(GObjectClass *klass)
{
	((EMFormatClass *)klass)->type_handlers = g_hash_table_new(g_str_hash, g_str_equal);
	emf_builtin_init((EMFormatClass *)klass);

	klass->finalize = emf_finalise;
	((EMFormatClass *)klass)->find_handler = emf_find_handler;
	((EMFormatClass *)klass)->format_clone = emf_format_clone;
	((EMFormatClass *)klass)->format_secure = emf_format_secure;
	((EMFormatClass *)klass)->busy = emf_busy;

	emf_signals[EMF_COMPLETE] =
		g_signal_new("complete",
			     G_OBJECT_CLASS_TYPE (klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET (EMFormatClass, complete),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
}

GType
em_format_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFormatClass),
			(GBaseInitFunc)emf_base_init, NULL,
			(GClassInitFunc)emf_class_init,
			NULL, NULL,
			sizeof(EMFormat), 0,
			(GInstanceInitFunc)emf_init
		};
		emf_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EMFormat", &info, 0);
	}

	return type;
}

/**
 * em_format_class_add_handler:
 * @emfc: EMFormatClass
 * @info: Callback information.
 *
 * Add a mime type handler to this class.  This is only used by
 * implementing classes.  The @info.old pointer will automatically be
 * setup to point to the old hanlder if one was already set.  This can
 * be used for overrides a fallback.
 *
 * When a mime type described by @info is encountered, the callback will
 * be invoked.  Note that @info may be extended by sub-classes if
 * they require additional context information.
 *
 * Use a mime type of "foo/ *" to insert a fallback handler for type "foo".
 **/
void
em_format_class_add_handler(EMFormatClass *emfc, EMFormatHandler *info)
{
	d(printf("adding format handler to '%s' '%s'\n",	g_type_name_from_class((GTypeClass *)emfc), info->mime_type));
	info->old = g_hash_table_lookup(emfc->type_handlers, info->mime_type);
	g_hash_table_insert(emfc->type_handlers, (gpointer) info->mime_type, info);
}

struct _class_handlers {
	EMFormatClass *old;
	EMFormatClass *new;
};
static void
merge_missing (gpointer key, gpointer value, gpointer userdata)
{
	struct _class_handlers *classes = (struct _class_handlers *) userdata;
	EMFormatHandler *info, *oldinfo;

	oldinfo = (EMFormatHandler *) value;
	info = g_hash_table_lookup (classes->new->type_handlers, key);
	if (!info) {
		/* Might be from a plugin */
		g_hash_table_insert (classes->new->type_handlers, key, value);
	}

}

void
em_format_merge_handler(EMFormat *new, EMFormat *old)
{
	EMFormatClass *oldc = (EMFormatClass *)G_OBJECT_GET_CLASS(old);
	EMFormatClass *newc = (EMFormatClass *)G_OBJECT_GET_CLASS(new);
	struct _class_handlers fclasses;

	fclasses.old = oldc;
	fclasses.new = newc;

	g_hash_table_foreach (oldc->type_handlers, merge_missing, &fclasses);

}
/**
 * em_format_class_remove_handler:
 * @emfc:
 * @info:
 *
 * Remove a handler.  @info must be a value which was previously
 * added.
 **/
void
em_format_class_remove_handler(EMFormatClass *emfc, EMFormatHandler *info)
{
	EMFormatHandler *current;

	/* TODO: thread issues? */

	current = g_hash_table_lookup(emfc->type_handlers, info->mime_type);
	if (current == info) {
		current = info->old;
		if (current)
			g_hash_table_insert(emfc->type_handlers, (gpointer) current->mime_type, current);
		else
			g_hash_table_remove(emfc->type_handlers, info->mime_type);
	} else {
		while (current && current->old != info)
			current = current->old;
		g_return_if_fail(current != NULL);
		current->old = info->old;
	}
}

/**
 * em_format_find_handler:
 * @emf:
 * @mime_type:
 *
 * Find a format handler by @mime_type.
 *
 * Return value: NULL if no handler is available.
 **/
static const EMFormatHandler *
emf_find_handler(EMFormat *emf, const gchar *mime_type)
{
	EMFormatClass *emfc = (EMFormatClass *)G_OBJECT_GET_CLASS(emf);

	return g_hash_table_lookup(emfc->type_handlers, mime_type);
}

/**
 * em_format_fallback_handler:
 * @emf:
 * @mime_type:
 *
 * Try to find a format handler based on the major type of the @mime_type.
 *
 * The subtype is replaced with "*" and a lookup performed.
 *
 * Return value:
 **/
const EMFormatHandler *
em_format_fallback_handler(EMFormat *emf, const gchar *mime_type)
{
	gchar *mime, *s;

	s = strchr(mime_type, '/');
	if (s == NULL)
		mime = (gchar *)mime_type;
	else {
		gsize len = (s-mime_type)+1;

		mime = alloca(len+2);
		strncpy(mime, mime_type, len);
		strcpy(mime+len, "*");
	}

	return em_format_find_handler(emf, mime);
}

/**
 * em_format_add_puri:
 * @emf:
 * @size:
 * @cid: Override the autogenerated content id.
 * @part:
 * @func:
 *
 * Add a pending-uri handler.  When formatting parts that reference
 * other parts, a pending-uri (PURI) can be used to track the reference.
 *
 * @size is used to allocate the structure, so that it can be directly
 * subclassed by implementors.
 *
 * @cid can be used to override the key used to retreive the PURI, if NULL,
 * then the content-location and the content-id of the @part are stored
 * as lookup keys for the part.
 *
 * FIXME: This may need a free callback.
 *
 * Return value: A new PURI, with a referenced copy of @part, and the cid
 * always set.  The uri will be set if one is available.  Clashes
 * are resolved by forgetting the old PURI in the global index.
 **/
EMFormatPURI *
em_format_add_puri(EMFormat *emf, gsize size, const gchar *cid, CamelMimePart *part, EMFormatPURIFunc func)
{
	EMFormatPURI *puri;
	const gchar *tmp;

	d(printf("adding puri for part: %s\n", emf->part_id->str));

	if (size < sizeof(*puri)) {
		g_warning (
			"size (%" G_GSIZE_FORMAT
			") less than size of puri\n", size);
		size = sizeof (*puri);
	}

	puri = g_malloc0(size);

	puri->format = emf;
	puri->func = func;
	puri->use_count = 0;
	puri->cid = g_strdup(cid);
	puri->part_id = g_strdup(emf->part_id->str);

	if (part) {
		camel_object_ref(part);
		puri->part = part;
	}

	if (part != NULL && cid == NULL) {
		tmp = camel_mime_part_get_content_id(part);
		if (tmp)
			puri->cid = g_strdup_printf("cid:%s", tmp);
		else
			puri->cid = g_strdup_printf("em-no-cid:%s", emf->part_id->str);

		d(printf("built cid '%s'\n", puri->cid));

		/* not quite same as old behaviour, it also put in the relative uri and a fallback for no parent uri */
		tmp = camel_mime_part_get_content_location(part);
		puri->uri = NULL;
		if (tmp == NULL) {
			/* no location, don't set a uri at all, html parts do this themselves */
		} else {
			if (strchr(tmp, ':') == NULL && emf->base != NULL) {
				CamelURL *uri;

				uri = camel_url_new_with_base(emf->base, tmp);
				puri->uri = camel_url_to_string(uri, 0);
				camel_url_free(uri);
			} else {
				puri->uri = g_strdup(tmp);
			}
		}
	}

	g_return_val_if_fail (puri->cid != NULL, NULL);
	g_return_val_if_fail (emf->pending_uri_level != NULL, NULL);
	g_return_val_if_fail (emf->pending_uri_table != NULL, NULL);

	e_dlist_addtail(&emf->pending_uri_level->uri_list, (EDListNode *)puri);

	if (puri->uri)
		g_hash_table_insert(emf->pending_uri_table, puri->uri, puri);
	g_hash_table_insert(emf->pending_uri_table, puri->cid, puri);

	return puri;
}

/**
 * em_format_push_level:
 * @emf:
 *
 * This is used to build a heirarchy of visible PURI objects based on
 * the structure of the message.  Used by multipart/alternative formatter.
 *
 * FIXME: This could probably also take a uri so it can automaticall update
 * the base location.
 **/
void
em_format_push_level(EMFormat *emf)
{
	struct _EMFormatPURITree *purilist;

	d(printf("em_format_push_level\n"));
	purilist = g_malloc0(sizeof(*purilist));
	e_dlist_init(&purilist->children);
	e_dlist_init(&purilist->uri_list);
	purilist->parent = emf->pending_uri_level;
	if (emf->pending_uri_tree == NULL) {
		emf->pending_uri_tree = purilist;
	} else {
		e_dlist_addtail(&emf->pending_uri_level->children, (EDListNode *)purilist);
	}
	emf->pending_uri_level = purilist;
}

/**
 * em_format_pull_level:
 * @emf:
 *
 * Drop a level of visibility back to the parent.  Note that
 * no PURI values are actually freed.
 **/
void
em_format_pull_level(EMFormat *emf)
{
	d(printf("em_format_pull_level\n"));
	emf->pending_uri_level = emf->pending_uri_level->parent;
}

/**
 * em_format_find_visible_puri:
 * @emf:
 * @uri:
 *
 * Search for a PURI based on the visibility defined by :push_level()
 * and :pull_level().
 *
 * Return value:
 **/
EMFormatPURI *
em_format_find_visible_puri(EMFormat *emf, const gchar *uri)
{
	EMFormatPURI *pw;
	struct _EMFormatPURITree *ptree;

	d(printf("checking for visible uri '%s'\n", uri));

	ptree = emf->pending_uri_level;
	while (ptree) {
		pw = (EMFormatPURI *)ptree->uri_list.head;
		while (pw->next) {
			d(printf(" pw->uri = '%s' pw->cid = '%s\n", pw->uri?pw->uri:"", pw->cid));
			if ((pw->uri && !strcmp(pw->uri, uri)) || !strcmp(pw->cid, uri))
				return pw;
			pw = pw->next;
		}
		ptree = ptree->parent;
	}

	return NULL;
}

/**
 * em_format_find_puri:
 * @emf:
 * @uri:
 *
 * Search for a PURI based on a uri.  Both the content-id
 * and content-location are checked.
 *
 * Return value:
 **/
EMFormatPURI *

em_format_find_puri(EMFormat *emf, const gchar *uri)
{
	return g_hash_table_lookup(emf->pending_uri_table, uri);
}

static void
emf_clear_puri_node(struct _EMFormatPURITree *node)
{
	{
		EMFormatPURI *pw, *pn;

		/* clear puri's at this level */
		pw = (EMFormatPURI *)node->uri_list.head;
		pn = pw->next;
		while (pn) {
			d(printf ("freeing pw %p format:%p\n", pw, pw->format));
			if (pw->free)
				pw->free(pw);
			g_free(pw->uri);
			g_free(pw->cid);
			g_free(pw->part_id);
			if (pw->part)
				camel_object_unref(pw->part);
			g_free(pw);
			pw = pn;
			pn = pn->next;
		}
	}

	{
		struct _EMFormatPURITree *cw, *cn;

		/* clear child nodes */
		cw = (struct _EMFormatPURITree *)node->children.head;
		cn = cw->next;
		while (cn) {
			emf_clear_puri_node(cw);
			cw = cn;
			cn = cn->next;
		}
	}

	g_free(node);
}

/**
 * em_format_clear_puri_tree:
 * @emf:
 *
 * For use by implementors to clear out the message structure
 * data.
 **/
void
em_format_clear_puri_tree(EMFormat *emf)
{
	d(printf("clearing pending uri's\n"));

	if (emf->pending_uri_table) {
		g_hash_table_destroy(emf->pending_uri_table);
		emf_clear_puri_node(emf->pending_uri_tree);
		emf->pending_uri_level = NULL;
		emf->pending_uri_tree = NULL;
	}
	emf->pending_uri_table = g_hash_table_new(g_str_hash, g_str_equal);
	em_format_push_level(emf);
}

/* use mime_type == NULL  to force showing as application/octet-stream */
void
em_format_part_as(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const gchar *mime_type)
{
	const EMFormatHandler *handle = NULL;
	const gchar *snoop_save = emf->snoop_mime_type, *tmp;
	CamelURL *base_save = emf->base, *base = NULL;
	gchar *basestr = NULL;

	d(printf("format_part_as()\n"));

	emf->snoop_mime_type = NULL;

	/* RFC 2110, we keep track of content-base, and absolute content-location headers
	   This is actually only required for html, but, *shrug* */
	tmp = camel_medium_get_header((CamelMedium *)part, "Content-Base");
	if (tmp == NULL) {
		tmp = camel_mime_part_get_content_location(part);
		if (tmp && strchr(tmp, ':') == NULL)
			tmp = NULL;
	} else {
		tmp = basestr = camel_header_location_decode(tmp);
	}
	d(printf("content-base is '%s'\n", tmp?tmp:"<unset>"));
	if (tmp
	    && (base = camel_url_new(tmp, NULL))) {
		emf->base = base;
		d(printf("Setting content base '%s'\n", tmp));
	}
	g_free(basestr);

	if (mime_type != NULL) {
		gboolean is_fallback = FALSE;
		if (g_ascii_strcasecmp(mime_type, "application/octet-stream") == 0) {
			emf->snoop_mime_type = mime_type = em_utils_snoop_type(part);
			if (mime_type == NULL)
				mime_type = "application/octet-stream";
		}

		handle = em_format_find_handler(emf, mime_type);
		if (handle == NULL) {
			handle = em_format_fallback_handler(emf, mime_type);
			is_fallback = TRUE;
		}

		if (handle != NULL
		    && !em_format_is_attachment(emf, part)) {
			d(printf("running handler for type '%s'\n", mime_type));
			if (is_fallback)
				camel_object_meta_set (part, "EMF-Fallback", "1");
			handle->handler(emf, stream, part, handle);
			if (is_fallback)
				camel_object_meta_set (part, "EMF-Fallback",  NULL);
			goto finish;
		}
		d(printf("this type is an attachment? '%s'\n", mime_type));
	} else {
		mime_type = "application/octet-stream";
	}

	((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_attachment(emf, stream, part, mime_type, handle);
finish:
	emf->base = base_save;
	emf->snoop_mime_type = snoop_save;

	if (base)
		camel_url_free(base);
}

void
em_format_part(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	gchar *mime_type;
	CamelDataWrapper *dw;

	dw = camel_medium_get_content_object((CamelMedium *)part);
	mime_type = camel_data_wrapper_get_mime_type(dw);
	if (mime_type) {
		camel_strdown(mime_type);
		em_format_part_as(emf, stream, part, mime_type);
		g_free(mime_type);
	} else
		em_format_part_as(emf, stream, part, "text/plain");
}

static void
emf_clone_inlines(gpointer key, gpointer val, gpointer data)
{
	struct _EMFormatCache *emfc = val, *new;

	new = emf_insert_cache((EMFormat *)data, emfc->partid);
	new->state = emfc->state;
	if (emfc->valid)
		new->valid = camel_cipher_validity_clone(emfc->valid);
	if (emfc->secured)
		camel_object_ref((new->secured = emfc->secured));
}

static void
emf_format_clone(EMFormat *emf, CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, EMFormat *emfsource)
{
	em_format_clear_puri_tree(emf);

	if (emf != emfsource) {
		g_hash_table_remove_all(emf->inline_table);
		if (emfsource) {
			struct _EMFormatHeader *h;

			/* We clone the current state here */
			g_hash_table_foreach(emfsource->inline_table, emf_clone_inlines, emf);
			emf->mode = emfsource->mode;
			g_free(emf->charset);
			emf->charset = g_strdup(emfsource->charset);
			g_free (emf->default_charset);
			emf->default_charset = g_strdup (emfsource->default_charset);

			em_format_clear_headers(emf);
			for (h = (struct _EMFormatHeader *)emfsource->header_list.head; h->next; h = h->next)
				em_format_add_header(emf, h->name, h->flags);
		}
	}

	/* what a mess */
	if (folder != emf->folder) {
		if (emf->folder)
			camel_object_unref(emf->folder);
		if (folder)
			camel_object_ref(folder);
		emf->folder = folder;
	}

	if (uid != emf->uid) {
		g_free(emf->uid);
		emf->uid = g_strdup(uid);
	}

	if (msg != emf->message) {
		if (emf->message)
			camel_object_unref(emf->message);
		if (msg)
			camel_object_ref(msg);
		emf->message = msg;
	}

	g_string_truncate(emf->part_id, 0);
	if (folder != NULL)
		/* TODO build some string based on the folder name/location? */
		g_string_append_printf(emf->part_id, ".%p", (gpointer) folder);
	if (uid != NULL)
		g_string_append_printf(emf->part_id, ".%s", uid);
}

static void
emf_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid)
{
	CamelCipherValidity *save = emf->valid_parent;
	gint len;

	/* Note that this also requires support from higher up in the class chain
	    - validity needs to be cleared when you start output
	    - also needs to be cleared (but saved) whenever you start a new message. */

	if (emf->valid == NULL) {
		emf->valid = valid;
	} else {
		camel_dlist_addtail(&emf->valid_parent->children, (CamelDListNode *)valid);
		camel_cipher_validity_envelope(emf->valid_parent, valid);
	}

	emf->valid_parent = valid;

	len = emf->part_id->len;
	g_string_append_printf(emf->part_id, ".secured");
	em_format_part(emf, stream, part);
	g_string_truncate(emf->part_id, len);

	emf->valid_parent = save;
}

static gboolean
emf_busy(EMFormat *emf)
{
	return FALSE;
}

/**
 * em_format_format_clone:
 * @emf: Mail formatter.
 * @folder: Camel Folder.
 * @uid: Uid of message.
 * @msg: Camel Message.
 * @emfsource: Used as a basis for user-altered layout, e.g. inline viewed
 * attachments.
 *
 * Format a message @msg.  If @emfsource is non NULL, then the status of
 * inlined expansion and so forth is copied direction from @emfsource.
 *
 * By passing the same value for @emf and @emfsource, you can perform
 * a display refresh, or it can be used to generate an identical layout,
 * e.g. to print what the user has shown inline.
 **/
void
em_format_format_clone (EMFormat *emf,
                        CamelFolder *folder,
                        const gchar *uid,
                        CamelMimeMessage *message,
                        EMFormat *source)
{
	EMFormatClass *class;

	g_return_if_fail (EM_IS_FORMAT (emf));
	g_return_if_fail (folder == NULL || CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message == NULL || CAMEL_IS_MIME_MESSAGE (message));
	g_return_if_fail (source == NULL || EM_IS_FORMAT (source));

	class = EM_FORMAT_GET_CLASS (emf);
	g_return_if_fail (class->format_clone != NULL);
	class->format_clone (emf, folder, uid, message, source);
}

void
em_format_format (EMFormat *emf,
                  CamelFolder *folder,
                  const gchar *uid,
                  CamelMimeMessage *message)
{
	/* em_format_format_clone() will check the arguments. */
	em_format_format_clone (emf, folder, uid, message, NULL);
}

void
em_format_redraw (EMFormat *emf)
{
	g_return_if_fail (EM_IS_FORMAT (emf));

	em_format_format_clone (
		emf, emf->folder, emf->uid, emf->message, emf);
}

/**
 * em_format_set_session:
 * @emf:
 * @s:
 *
 * Set the CamelSession to be used for signature verification and decryption
 * purposes.  If this is not set, then signatures cannot be verified or
 * encrypted messages viewed.
 **/
void
em_format_set_session(EMFormat *emf, CamelSession *s)
{
	if (s)
		camel_object_ref(s);
	if (emf->session)
		camel_object_unref(emf->session);
	emf->session = s;
}

/**
 * em_format_set_mode:
 * @emf:
 * @type:
 *
 * Set display mode, EM_FORMAT_SOURCE, EM_FORMAT_ALLHEADERS, or
 * EM_FORMAT_NORMAL.
 **/
void
em_format_set_mode(EMFormat *emf, em_format_mode_t type)
{
	if (emf->mode == type)
		return;

	emf->mode = type;

	/* force redraw if type changed afterwards */
	if (emf->message)
		em_format_redraw(emf);
}

/**
 * em_format_set_charset:
 * @emf:
 * @charset:
 *
 * set override charset on formatter.  message will be redisplayed if
 * required.
 **/
void
em_format_set_charset(EMFormat *emf, const gchar *charset)
{
	if ((emf->charset && charset && g_ascii_strcasecmp(emf->charset, charset) == 0)
	    || (emf->charset == NULL && charset == NULL)
	    || (emf->charset == charset))
		return;

	g_free(emf->charset);
	emf->charset = g_strdup(charset);

	if (emf->message)
		em_format_redraw(emf);
}

/**
 * em_format_set_default_charset:
 * @emf:
 * @charset:
 *
 * Set the fallback, default system charset to use when no other charsets
 * are present.  Message will be redisplayed if required (and sometimes redisplayed
 * when it isn't).
 **/
void
em_format_set_default_charset(EMFormat *emf, const gchar *charset)
{
	if ((emf->default_charset && charset && g_ascii_strcasecmp(emf->default_charset, charset) == 0)
	    || (emf->default_charset == NULL && charset == NULL)
	    || (emf->default_charset == charset))
		return;

	g_free(emf->default_charset);
	emf->default_charset = g_strdup(charset);

	if (emf->message && emf->charset == NULL)
		em_format_redraw(emf);
}

/**
 * em_format_clear_headers:
 * @emf:
 *
 * Clear the list of headers to be displayed.  This will force all headers to
 * be shown.
 **/
void
em_format_clear_headers(EMFormat *emf)
{
	EMFormatHeader *eh;

	while ((eh = (EMFormatHeader *)e_dlist_remhead(&emf->header_list)))
		g_free(eh);
}

/* note: also copied in em-mailer-prefs.c */
static const struct {
	const gchar *name;
	guint32 flags;
} default_headers[] = {
	{ N_("From"), EM_FORMAT_HEADER_BOLD },
	{ N_("Reply-To"), EM_FORMAT_HEADER_BOLD },
	{ N_("To"), EM_FORMAT_HEADER_BOLD },
	{ N_("Cc"), EM_FORMAT_HEADER_BOLD },
	{ N_("Bcc"), EM_FORMAT_HEADER_BOLD },
	{ N_("Subject"), EM_FORMAT_HEADER_BOLD },
	{ N_("Date"), EM_FORMAT_HEADER_BOLD },
	{ N_("Newsgroups"), EM_FORMAT_HEADER_BOLD },
	{ N_("Face"), 0 },
};

/**
 * em_format_default_headers:
 * @emf:
 *
 * Set the headers to show to the default list.
 *
 * From, Reply-To, To, Cc, Bcc, Subject and Date.
 **/
void
em_format_default_headers(EMFormat *emf)
{
	gint i;

	em_format_clear_headers(emf);
	for (i=0; i<sizeof(default_headers)/sizeof(default_headers[0]); i++)
		em_format_add_header(emf, default_headers[i].name, default_headers[i].flags);
}

/**
 * em_format_add_header:
 * @emf:
 * @name: The name of the header, as it will appear during output.
 * @flags: EM_FORMAT_HEAD_* defines to control display attributes.
 *
 * Add a specific header to show.  If any headers are set, they will
 * be displayed in the order set by this function.  Certain known
 * headers included in this list will be shown using special
 * formatting routines.
 **/
void em_format_add_header(EMFormat *emf, const gchar *name, guint32 flags)
{
	EMFormatHeader *h;

	h = g_malloc(sizeof(*h) + strlen(name));
	h->flags = flags;
	strcpy(h->name, name);
	e_dlist_addtail(&emf->header_list, (EDListNode *)h);
}

/**
 * em_format_is_attachment:
 * @emf:
 * @part: Part to check.
 *
 * Returns true if the part is an attachment.
 *
 * A part is not considered an attachment if it is a
 * multipart, or a text part with no filename.  It is used
 * to determine if an attachment header should be displayed for
 * the part.
 *
 * Content-Disposition is not checked.
 *
 * Return value: TRUE/FALSE
 **/
gint em_format_is_attachment(EMFormat *emf, CamelMimePart *part)
{
	/*CamelContentType *ct = camel_mime_part_get_content_type(part);*/
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)part);

	if (!dw)
		return 0;

	/*printf("checking is attachment %s/%s\n", ct->type, ct->subtype);*/
	return !(camel_content_type_is (dw->mime_type, "multipart", "*")
		 || camel_content_type_is(dw->mime_type, "application", "x-pkcs7-mime")
		 || camel_content_type_is(dw->mime_type, "application", "pkcs7-mime")
		 || camel_content_type_is(dw->mime_type, "application", "x-inlinepgp-signed")
		 || camel_content_type_is(dw->mime_type, "application", "x-inlinepgp-encrypted")
		 || camel_content_type_is(dw->mime_type, "x-evolution", "evolution-rss-feed")
		 || (camel_content_type_is (dw->mime_type, "text", "*")
		     && camel_mime_part_get_filename(part) == NULL));
}

/**
 * em_format_is_inline:
 * @emf:
 * @part:
 * @partid: format->part_id part id of this part.
 * @handle: handler for this part
 *
 * Returns true if the part should be displayed inline.  Any part with
 * a Content-Disposition of inline, or if the @handle has a default
 * inline set, will be shown inline.
 *
 * :set_inline() called on the same part will override any calculated
 * value.
 *
 * Return value:
 **/
gint em_format_is_inline(EMFormat *emf, const gchar *partid, CamelMimePart *part, const EMFormatHandler *handle)
{
	struct _EMFormatCache *emfc;
	const gchar *tmp;

	if (handle == NULL)
		return FALSE;

	emfc = g_hash_table_lookup(emf->inline_table, partid);
	if (emfc && emfc->state != INLINE_UNSET)
		return emfc->state & 1;

	/* some types need to override the disposition, e.g. application/x-pkcs7-mime */
	if (handle->flags & EM_FORMAT_HANDLER_INLINE_DISPOSITION)
		return TRUE;

	tmp = camel_mime_part_get_disposition(part);
	if (tmp)
		return g_ascii_strcasecmp(tmp, "inline") == 0;

	/* otherwise, use the default for this handler type */
	return (handle->flags & EM_FORMAT_HANDLER_INLINE) != 0;
}

/**
 * em_format_set_inline:
 * @emf:
 * @partid: id of part
 * @state:
 *
 * Force the attachment @part to be expanded or hidden explictly to match
 * @state.  This is used only to record the change for a redraw or
 * cloned layout render and does not force a redraw.
 **/
void em_format_set_inline(EMFormat *emf, const gchar *partid, gint state)
{
	struct _EMFormatCache *emfc;

	emfc = g_hash_table_lookup(emf->inline_table, partid);
	if (emfc == NULL) {
		emfc = emf_insert_cache(emf, partid);
	} else if (emfc->state != INLINE_UNSET && (emfc->state & 1) == state)
		return;

	emfc->state = state?INLINE_ON:INLINE_OFF;

	if (emf->message)
		em_format_redraw(emf);
}

void em_format_format_error(EMFormat *emf, CamelStream *stream, const gchar *fmt, ...)
{
	va_list ap;
	gchar *txt;

	va_start(ap, fmt);
	txt = g_strdup_vprintf(fmt, ap);
	((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_error((emf), (stream), (txt));
	g_free(txt);
}

void
em_format_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid)
{
	((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_secure(emf, stream, part, valid);

	if (emf->valid_parent == NULL && emf->valid != NULL) {
		camel_cipher_validity_free(emf->valid);
		emf->valid = NULL;
	}
}

/* should this be virtual? */
void
em_format_format_content(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)part);

	if (camel_content_type_is (dw->mime_type, "text", "*"))
		em_format_format_text(emf, stream, (CamelDataWrapper *)part);
	else
		camel_data_wrapper_decode_to_stream(dw, stream);
}

/**
 * em_format_format_content:
 * @emf:
 * @stream: Where to write the converted text
 * @part: Part whose container is to be formatted
 *
 * Decode/output a part's content to @stream.
 **/
void
em_format_format_text(EMFormat *emf, CamelStream *stream, CamelDataWrapper *dw)
{
	CamelStreamFilter *filter_stream;
	CamelMimeFilterCharset *filter;
	const gchar *charset = NULL;
	CamelMimeFilterWindows *windows = NULL;
	CamelStream *mem_stream = NULL;
	gsize size;
	gsize max;

	if (emf->charset) {
		charset = emf->charset;
	} else if (dw->mime_type
		   && (charset = camel_content_type_param (dw->mime_type, "charset"))
		   && g_ascii_strncasecmp(charset, "iso-8859-", 9) == 0) {
		CamelStream *null;

		/* Since a few Windows mailers like to claim they sent
		 * out iso-8859-# encoded text when they really sent
		 * out windows-cp125#, do some simple sanity checking
		 * before we move on... */

		null = camel_stream_null_new();
		filter_stream = camel_stream_filter_new_with_stream(null);
		camel_object_unref(null);

		windows = (CamelMimeFilterWindows *)camel_mime_filter_windows_new(charset);
		camel_stream_filter_add(filter_stream, (CamelMimeFilter *)windows);

		camel_data_wrapper_decode_to_stream(dw, (CamelStream *)filter_stream);
		camel_stream_flush((CamelStream *)filter_stream);
		camel_object_unref(filter_stream);

		charset = camel_mime_filter_windows_real_charset (windows);
	} else if (charset == NULL) {
		charset = emf->default_charset;
	}

	mem_stream = (CamelStream *)camel_stream_mem_new ();
	filter_stream = camel_stream_filter_new_with_stream(mem_stream);

	if ((filter = camel_mime_filter_charset_new_convert(charset, "UTF-8"))) {
		camel_stream_filter_add(filter_stream, (CamelMimeFilter *) filter);
		camel_object_unref(filter);
	}

	max = mail_config_get_message_limit();
	size = camel_data_wrapper_decode_to_stream(emf->mode == EM_FORMAT_SOURCE ? (CamelDataWrapper *)dw: camel_medium_get_content_object((CamelMedium *)dw), (CamelStream *)filter_stream);
	camel_stream_flush((CamelStream *)filter_stream);
	camel_object_unref(filter_stream);
	camel_stream_reset (mem_stream);

	if (max == -1 || size == -1 || size < (max * 1024) || emf->composer) {
		camel_stream_write_to_stream(mem_stream, (CamelStream *)stream);
		camel_stream_flush((CamelStream *)stream);
	} else {
		((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_optional(emf, stream, (CamelMimePart *)dw, mem_stream);
	}

	if (windows)
		camel_object_unref(windows);
}

/**
 * em_format_describe_part:
 * @part:
 * @mimetype:
 *
 * Generate a simple textual description of a part, @mime_type represents the
 * the content.
 *
 * Return value:
 **/
gchar *
em_format_describe_part(CamelMimePart *part, const gchar *mime_type)
{
	GString *stext;
	const gchar *filename, *description;
	gchar *content_type, *desc;

	stext = g_string_new("");
	content_type = g_content_type_from_mime_type (mime_type);
	desc = g_content_type_get_description (content_type ? content_type : mime_type);
	g_free (content_type);
	g_string_append_printf (stext, _("%s attachment"), desc ? desc : mime_type);
	g_free (desc);
	if ((filename = camel_mime_part_get_filename (part)))
		g_string_append_printf(stext, " (%s)", filename);
	if ((description = camel_mime_part_get_description(part)) &&
		(*description != 0) &&
		!(filename && (strcmp(filename, description) == 0)))
		g_string_append_printf(stext, ", \"%s\"", description);

	return g_string_free (stext, FALSE);
}

/* ********************************************************************** */

#ifdef ENABLE_SMIME
static void
emf_application_xpkcs7mime(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelCipherContext *context;
	CamelException *ex;
	CamelMimePart *opart;
	CamelCipherValidity *valid;
	struct _EMFormatCache *emfc;

	/* should this perhaps run off a key of ".secured" ? */
	emfc = g_hash_table_lookup(emf->inline_table, emf->part_id->str);
	if (emfc && emfc->valid) {
		em_format_format_secure(emf, stream, emfc->secured, camel_cipher_validity_clone(emfc->valid));
		return;
	}

	ex = camel_exception_new();

	context = camel_smime_context_new(session);

	opart = camel_mime_part_new();
	valid = camel_cipher_decrypt(context, part, opart, ex);
	if (valid == NULL) {
		em_format_format_error(emf, stream, "%s", ex->desc?ex->desc:_("Could not parse S/MIME message: Unknown error"));
		em_format_part_as(emf, stream, part, NULL);
	} else {
		if (emfc == NULL)
			emfc = emf_insert_cache(emf, emf->part_id->str);

		emfc->valid = camel_cipher_validity_clone(valid);
		camel_object_ref((emfc->secured = opart));

		em_format_format_secure(emf, stream, opart, valid);
	}

	camel_object_unref(opart);
	camel_object_unref(context);
	camel_exception_free(ex);
}
#endif

/* RFC 1740 */
static void
emf_multipart_appledouble(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	CamelMimePart *mime_part;
	gint len;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	mime_part = camel_multipart_get_part(mp, 1);
	if (mime_part) {
		/* try the data fork for something useful, doubtful but who knows */
		len = emf->part_id->len;
		g_string_append_printf(emf->part_id, ".appledouble.1");
		em_format_part(emf, stream, mime_part);
		g_string_truncate(emf->part_id, len);
	} else
		em_format_format_source(emf, stream, part);

}

/* RFC ??? */
static void
emf_multipart_mixed(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	gint i, nparts, len;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	len = emf->part_id->len;
	nparts = camel_multipart_get_number(mp);
	for (i = 0; i < nparts; i++) {
		part = camel_multipart_get_part(mp, i);
		g_string_append_printf(emf->part_id, ".mixed.%d", i);
		em_format_part(emf, stream, part);
		g_string_truncate(emf->part_id, len);
	}
}

/* RFC 1740 */
static void
emf_multipart_alternative(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	gint i, nparts, bestid = 0;
	CamelMimePart *best = NULL;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	/* as per rfc, find the last part we know how to display */
	nparts = camel_multipart_get_number(mp);
	for (i = 0; i < nparts; i++) {
		CamelContentType *type;
		gchar *mime_type;

		/* is it correct to use the passed in *part here? */
		part = camel_multipart_get_part(mp, i);

		if (!part)
			continue;

		type = camel_mime_part_get_content_type (part);
		mime_type = camel_content_type_simple (type);

		camel_strdown (mime_type);

		/*if (want_plain && !strcmp (mime_type, "text/plain"))
		  return part;*/

		if (em_format_find_handler(emf, mime_type)
		    || (best == NULL && em_format_fallback_handler(emf, mime_type))) {
			best = part;
			bestid = i;
		}

		g_free(mime_type);
	}

	if (best) {
		gint len = emf->part_id->len;

		g_string_append_printf(emf->part_id, ".alternative.%d", bestid);
		em_format_part(emf, stream, best);
		g_string_truncate(emf->part_id, len);
	} else
		emf_multipart_mixed(emf, stream, part, info);
}

static void
emf_multipart_encrypted(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelCipherContext *context;
	CamelException *ex;
	const gchar *protocol;
	CamelMimePart *opart;
	CamelCipherValidity *valid;
	CamelMultipartEncrypted *mpe;
	struct _EMFormatCache *emfc;

	/* should this perhaps run off a key of ".secured" ? */
	emfc = g_hash_table_lookup(emf->inline_table, emf->part_id->str);
	if (emfc && emfc->valid) {
		em_format_format_secure(emf, stream, emfc->secured, camel_cipher_validity_clone(emfc->valid));
		return;
	}

	mpe = (CamelMultipartEncrypted*)camel_medium_get_content_object((CamelMedium *)part);
	if (!CAMEL_IS_MULTIPART_ENCRYPTED(mpe)) {
		em_format_format_error(emf, stream, _("Could not parse MIME message. Displaying as source."));
		em_format_format_source(emf, stream, part);
		return;
	}

	/* Currently we only handle RFC2015-style PGP encryption. */
	protocol = camel_content_type_param(((CamelDataWrapper *)mpe)->mime_type, "protocol");
	if (!protocol || g_ascii_strcasecmp (protocol, "application/pgp-encrypted") != 0) {
		em_format_format_error(emf, stream, _("Unsupported encryption type for multipart/encrypted"));
		em_format_part_as(emf, stream, part, "multipart/mixed");
		return;
	}

	ex = camel_exception_new();
	context = camel_gpg_context_new(emf->session);
	opart = camel_mime_part_new();
	valid = camel_cipher_decrypt(context, part, opart, ex);
	if (valid == NULL) {
		em_format_format_error(emf, stream, ex->desc?_("Could not parse PGP/MIME message"):_("Could not parse PGP/MIME message: Unknown error"));
		if (ex->desc)
			em_format_format_error(emf, stream, "%s", ex->desc);
		em_format_part_as(emf, stream, part, "multipart/mixed");
	} else {
		if (emfc == NULL)
			emfc = emf_insert_cache(emf, emf->part_id->str);

		emfc->valid = camel_cipher_validity_clone(valid);
		camel_object_ref((emfc->secured = opart));

		em_format_format_secure(emf, stream, opart, valid);
	}

	/* TODO: Make sure when we finalise this part, it is zero'd out */
	camel_object_unref(opart);
	camel_object_unref(context);
	camel_exception_free(ex);
}

static void
emf_write_related(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	em_format_format_content(emf, stream, puri->part);
	camel_stream_close(stream);
}

/* RFC 2387 */
static void
emf_multipart_related(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	CamelMimePart *body_part, *display_part = NULL;
	CamelContentType *content_type;
	const gchar *start;
	gint i, nparts, partidlen, displayid = 0;
	gchar *oldpartid;
	struct _EMFormatPURITree *ptree;
	EMFormatPURI *puri, *purin;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	/* FIXME: put this stuff in a shared function */
	nparts = camel_multipart_get_number(mp);
	content_type = camel_mime_part_get_content_type(part);
	start = camel_content_type_param (content_type, "start");
	if (start && strlen(start)>2) {
		gint len;
		const gchar *cid;

		/* strip <>'s */
		len = strlen (start) - 2;
		start++;

		for (i=0; i<nparts; i++) {
			body_part = camel_multipart_get_part(mp, i);
			cid = camel_mime_part_get_content_id(body_part);

			if (cid && !strncmp(cid, start, len) && strlen(cid) == len) {
				display_part = body_part;
				displayid = i;
				break;
			}
		}
	} else {
		display_part = camel_multipart_get_part(mp, 0);
	}

	if (display_part == NULL) {
		emf_multipart_mixed(emf, stream, part, info);
		return;
	}

	em_format_push_level(emf);

	oldpartid = g_strdup(emf->part_id->str);
	partidlen = emf->part_id->len;

	/* queue up the parts for possible inclusion */
	for (i = 0; i < nparts; i++) {
		body_part = camel_multipart_get_part(mp, i);
		if (body_part != display_part) {
			/* set the partid since add_puri uses it */
			g_string_append_printf(emf->part_id, ".related.%d", i);
			puri = em_format_add_puri(emf, sizeof(EMFormatPURI), NULL, body_part, emf_write_related);
			g_string_truncate(emf->part_id, partidlen);
			d(printf(" part '%s' '%s' added\n", puri->uri?puri->uri:"", puri->cid));
		}
	}

	g_string_append_printf(emf->part_id, ".related.%d", displayid);
	em_format_part(emf, stream, display_part);
	g_string_truncate(emf->part_id, partidlen);
	camel_stream_flush(stream);

	ptree = emf->pending_uri_level;
	puri = (EMFormatPURI *)ptree->uri_list.head;
	purin = puri->next;
	while (purin) {
		if (puri->use_count == 0) {
			d(printf("part '%s' '%s' used '%d'\n", puri->uri?puri->uri:"", puri->cid, puri->use_count));
			if (puri->func == emf_write_related) {
				g_string_printf(emf->part_id, "%s", puri->part_id);
				em_format_part(emf, stream, puri->part);
			} else {
				d(printf("unreferenced uri generated by format code: %s\n", puri->uri?puri->uri:puri->cid));
			}
		}
		puri = purin;
		purin = purin->next;
	}

	g_string_printf(emf->part_id, "%s", oldpartid);
	g_free(oldpartid);

	em_format_pull_level(emf);
}

static void
emf_multipart_signed(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMimePart *cpart;
	CamelMultipartSigned *mps;
	CamelCipherContext *cipher = NULL;
	struct _EMFormatCache *emfc;

	/* should this perhaps run off a key of ".secured" ? */
	emfc = g_hash_table_lookup(emf->inline_table, emf->part_id->str);
	if (emfc && emfc->valid) {
		em_format_format_secure(emf, stream, emfc->secured, camel_cipher_validity_clone(emfc->valid));
		return;
	}

	mps = (CamelMultipartSigned *)camel_medium_get_content_object((CamelMedium *)part);
	if (!CAMEL_IS_MULTIPART_SIGNED(mps)
	    || (cpart = camel_multipart_get_part((CamelMultipart *)mps, CAMEL_MULTIPART_SIGNED_CONTENT)) == NULL) {
		em_format_format_error(emf, stream, _("Could not parse MIME message. Displaying as source."));
		em_format_format_source(emf, stream, part);
		return;
	}

	/* FIXME: Should be done via a plugin interface */
	/* FIXME: duplicated in em-format-html-display.c */
	if (mps->protocol) {
#ifdef ENABLE_SMIME
		if (g_ascii_strcasecmp("application/x-pkcs7-signature", mps->protocol) == 0
		    || g_ascii_strcasecmp("application/pkcs7-signature", mps->protocol) == 0)
			cipher = camel_smime_context_new(emf->session);
		else
#endif
			if (g_ascii_strcasecmp("application/pgp-signature", mps->protocol) == 0)
				cipher = camel_gpg_context_new(emf->session);
	}

	if (cipher == NULL) {
		em_format_format_error(emf, stream, _("Unsupported signature format"));
		em_format_part_as(emf, stream, part, "multipart/mixed");
	} else {
		CamelException *ex = camel_exception_new();
		CamelCipherValidity *valid;

		valid = camel_cipher_verify(cipher, part, ex);
		if (valid == NULL) {
			em_format_format_error(emf, stream, ex->desc?_("Error verifying signature"):_("Unknown error verifying signature"));
			if (ex->desc)
				em_format_format_error(emf, stream, "%s", ex->desc);
			em_format_part_as(emf, stream, part, "multipart/mixed");
		} else {
			if (emfc == NULL)
				emfc = emf_insert_cache(emf, emf->part_id->str);

			emfc->valid = camel_cipher_validity_clone(valid);
			camel_object_ref((emfc->secured = cpart));

			em_format_format_secure(emf, stream, cpart, valid);
		}

		camel_exception_free(ex);
		camel_object_unref(cipher);
	}
}

/* RFC 4155 */
static void
emf_application_mbox (EMFormat *emf,
                      CamelStream *stream,
                      CamelMimePart *mime_part,
                      const EMFormatHandler *info)
{
	const EMFormatHandler *handle;
	CamelMimeParser *parser;
	CamelStream *mem_stream;
	camel_mime_parser_state_t state;

	/* Extract messages from the application/mbox part and
	 * render them as a flat list of messages. */

	/* XXX If the mbox has multiple messages, maybe render them
	 *     as a multipart/digest so each message can be expanded
	 *     or collapsed individually.
	 *
	 *     See attachment_handler_mail_x_uid_list() for example. */

	/* XXX This is based on em_utils_read_messages_from_stream().
	 *     Perhaps refactor that function to return an array of
	 *     messages instead of assuming we want to append them
	 *     to a folder? */

	handle = em_format_find_handler (emf, "x-evolution/message/rfc822");
	g_return_if_fail (handle != NULL);

	parser = camel_mime_parser_new ();
	camel_mime_parser_scan_from (parser, TRUE);

	mem_stream = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream (
		CAMEL_DATA_WRAPPER (mime_part), mem_stream);
	camel_seekable_stream_seek (
		CAMEL_SEEKABLE_STREAM (mem_stream), 0, CAMEL_STREAM_SET);
	camel_mime_parser_init_with_stream (parser, mem_stream);
	camel_object_unref (mem_stream);

	/* Extract messages from the mbox. */
	state = camel_mime_parser_step (parser, NULL, NULL);
	while (state == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *message;

		message = camel_mime_message_new ();
		mime_part = CAMEL_MIME_PART (message);

		if (camel_mime_part_construct_from_parser (mime_part, parser) == -1) {
			camel_object_unref (message);
			break;
		}

		/* Render the message. */
		handle->handler (emf, stream, mime_part, handle);

		camel_object_unref (message);

		/* Skip past CAMEL_MIME_PARSER_STATE_FROM_END. */
		state = camel_mime_parser_step (parser, NULL, NULL);

		state = camel_mime_parser_step (parser, NULL, NULL);
	}

	camel_object_unref (parser);
}

static void
emf_message_rfc822(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)part);
	const EMFormatHandler *handle;
	gint len;

	if (!CAMEL_IS_MIME_MESSAGE(dw)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	len = emf->part_id->len;
	g_string_append_printf(emf->part_id, ".rfc822");

	handle = em_format_find_handler(emf, "x-evolution/message/rfc822");
	if (handle)
		handle->handler(emf, stream, (CamelMimePart *)dw, handle);

	g_string_truncate(emf->part_id, len);
}

static void
emf_message_deliverystatus(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	em_format_format_text(emf, stream, (CamelDataWrapper *)part);
}

static void
emf_inlinepgp_signed(EMFormat *emf, CamelStream *stream, CamelMimePart *ipart, EMFormatHandler *info)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilterPgp *pgp_filter;
	CamelContentType *content_type;
	CamelCipherContext *cipher;
	CamelCipherValidity *valid;
	CamelDataWrapper *dw;
	CamelMimePart *opart;
	CamelStream *ostream;
	CamelException *ex;
	gchar *type;

	if (!ipart) {
		em_format_format_error(emf, stream, _("Unknown error verifying signature"));
		return;
	}

	ex = camel_exception_new();
	cipher = camel_gpg_context_new(emf->session);
	/* Verify the signature of the message */
	valid = camel_cipher_verify(cipher, ipart, ex);
	if (!valid) {
		em_format_format_error(emf, stream, ex->desc?_("Error verifying signature"):_("Unknown error verifying signature"));
		if (ex->desc)
			em_format_format_error(emf, stream, "%s", ex->desc);
		em_format_format_source(emf, stream, ipart);
		/* I think this will loop: em_format_part_as(emf, stream, part, "text/plain"); */
		camel_exception_free(ex);
		camel_object_unref(cipher);
		return;
	}

	/* Setup output stream */
	ostream = camel_stream_mem_new();
	filtered_stream = camel_stream_filter_new_with_stream(ostream);

	/* Add PGP header / footer filter */
	pgp_filter = (CamelMimeFilterPgp *)camel_mime_filter_pgp_new();
	camel_stream_filter_add(filtered_stream, (CamelMimeFilter *)pgp_filter);
	camel_object_unref(pgp_filter);

	/* Pass through the filters that have been setup */
	dw = camel_medium_get_content_object((CamelMedium *)ipart);
	camel_data_wrapper_decode_to_stream(dw, (CamelStream *)filtered_stream);
	camel_stream_flush((CamelStream *)filtered_stream);
	camel_object_unref(filtered_stream);

	/* Create a new text/plain MIME part containing the signed content preserving the original part's Content-Type params */
	content_type = camel_mime_part_get_content_type (ipart);
	type = camel_content_type_format (content_type);
	content_type = camel_content_type_decode (type);
	g_free (type);

	g_free (content_type->type);
	content_type->type = g_strdup ("text");
	g_free (content_type->subtype);
	content_type->subtype = g_strdup ("plain");
	type = camel_content_type_format (content_type);
	camel_content_type_unref (content_type);

	dw = camel_data_wrapper_new ();
	camel_data_wrapper_construct_from_stream (dw, ostream);
	camel_data_wrapper_set_mime_type (dw, type);
	g_free (type);

	opart = camel_mime_part_new ();
	camel_medium_set_content_object ((CamelMedium *) opart, dw);
	camel_data_wrapper_set_mime_type_field ((CamelDataWrapper *) opart, dw->mime_type);

	/* Pass it off to the real formatter */
	em_format_format_secure(emf, stream, opart, valid);

	/* Clean Up */
	camel_object_unref(dw);
	camel_object_unref(opart);
	camel_object_unref(ostream);
	camel_object_unref(cipher);
	camel_exception_free(ex);
}

static void
emf_inlinepgp_encrypted(EMFormat *emf, CamelStream *stream, CamelMimePart *ipart, EMFormatHandler *info)
{
	CamelCipherContext *cipher;
	CamelCipherValidity *valid;
	CamelException *ex;
	CamelMimePart *opart;
	CamelDataWrapper *dw;
	gchar *mime_type;

	cipher = camel_gpg_context_new(emf->session);
	ex = camel_exception_new();
	opart = camel_mime_part_new();
	/* Decrypt the message */
	valid = camel_cipher_decrypt (cipher, ipart, opart, ex);
	if (!valid) {
		em_format_format_error(emf, stream, ex->desc?_("Could not parse PGP message"):_("Could not parse PGP message: Unknown error"));
		if (ex->desc)
			em_format_format_error(emf, stream, "%s", ex->desc);
		em_format_format_source(emf, stream, ipart);
		/* I think this will loop: em_format_part_as(emf, stream, part, "text/plain"); */
		camel_exception_free(ex);
		camel_object_unref(cipher);
		camel_object_unref(opart);
		return;
	}

	dw = camel_medium_get_content_object ((CamelMedium *)opart);
	mime_type = camel_data_wrapper_get_mime_type (dw);

	/* this ensures to show the 'opart' as inlined, if possible */
	if (mime_type && g_ascii_strcasecmp (mime_type, "application/octet-stream") == 0) {
		const gchar *snoop = em_utils_snoop_type (opart);

		if (snoop)
			camel_data_wrapper_set_mime_type (dw, snoop);
	}

	g_free (mime_type);

	/* Pass it off to the real formatter */
	em_format_format_secure(emf, stream, opart, valid);

	/* Clean Up */
	camel_object_unref(opart);
	camel_object_unref (cipher);
	camel_exception_free (ex);
}

static EMFormatHandler type_builtin_table[] = {
#ifdef ENABLE_SMIME
	{ (gchar *) "application/x-pkcs7-mime", (EMFormatFunc)emf_application_xpkcs7mime, EM_FORMAT_HANDLER_INLINE_DISPOSITION },
#endif
	{ (gchar *) "application/mbox", emf_application_mbox, EM_FORMAT_HANDLER_INLINE },
	{ (gchar *) "multipart/alternative", emf_multipart_alternative },
	{ (gchar *) "multipart/appledouble", emf_multipart_appledouble },
	{ (gchar *) "multipart/encrypted", emf_multipart_encrypted },
	{ (gchar *) "multipart/mixed", emf_multipart_mixed },
	{ (gchar *) "multipart/signed", emf_multipart_signed },
	{ (gchar *) "multipart/related", emf_multipart_related },
	{ (gchar *) "multipart/*", emf_multipart_mixed },
	{ (gchar *) "message/rfc822", emf_message_rfc822, EM_FORMAT_HANDLER_INLINE },
	{ (gchar *) "message/news", emf_message_rfc822, EM_FORMAT_HANDLER_INLINE },
	{ (gchar *) "message/delivery-status", emf_message_deliverystatus },
	{ (gchar *) "message/*", emf_message_rfc822, EM_FORMAT_HANDLER_INLINE },

	/* Insert brokenly-named parts here */
#ifdef ENABLE_SMIME
	{ (gchar *) "application/pkcs7-mime", (EMFormatFunc)emf_application_xpkcs7mime, EM_FORMAT_HANDLER_INLINE_DISPOSITION },
#endif

	/* internal types */
	{ (gchar *) "application/x-inlinepgp-signed", (EMFormatFunc)emf_inlinepgp_signed },
	{ (gchar *) "application/x-inlinepgp-encrypted", (EMFormatFunc)emf_inlinepgp_encrypted },
};

static void
emf_builtin_init(EMFormatClass *klass)
{
	gint i;

	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		g_hash_table_insert(klass->type_handlers, (gpointer) type_builtin_table[i].mime_type, &type_builtin_table[i]);
}
