/*
 * Code for autogenerating rules or filters from a message.
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
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "mail-vfolder.h"
#include "mail-autofilter.h"
#include "mail-component.h"
#include "em-utils.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#include "em-vfolder-context.h"
#include "em-vfolder-rule.h"
#include "em-vfolder-editor.h"

#include "em-filter-context.h"
#include "em-filter-rule.h"
#include "em-filter-editor.h"
#include "filter/filter-option.h"

#include <camel/camel-internet-address.h>
#include <camel/camel-mime-message.h>

#define d(x)

static void
rule_match_recipients (RuleContext *context, FilterRule *rule, CamelInternetAddress *iaddr)
{
	FilterPart *part;
	FilterElement *element;
	gint i;
	const gchar *real, *addr;
	gchar *namestr;

	/* address types etc should handle multiple values */
	for (i = 0; camel_internet_address_get (iaddr, i, &real, &addr); i++) {
		part = rule_context_create_part (context, "to");
		filter_rule_add_part ((FilterRule *)rule, part);
		element = filter_part_find_element (part, "recipient-type");
		filter_option_set_current ((FilterOption *)element, "contains");
		element = filter_part_find_element (part, "recipient");
		filter_input_set_value ((FilterInput *)element, addr);

		namestr = g_strdup_printf (_("Mail to %s"), real && real[0] ? real : addr);
		filter_rule_set_name (rule, namestr);
		g_free (namestr);
	}
}

/* remove 're' part of a subject */
static const gchar *
strip_re (const gchar *subject)
{
	const guchar *s, *p;

	s = (guchar *) subject;

	while (*s) {
		while (isspace (*s))
			s++;
		if (s[0] == 0)
			break;
		if ((s[0] == 'r' || s[0] == 'R')
		    && (s[1] == 'e' || s[1] == 'E')) {
			p = s+2;
			while (isdigit (*p) || (ispunct (*p) && (*p != ':')))
				p++;
			if (*p == ':') {
				s = p + 1;
			} else
				break;
		} else
			break;
	}

	return (gchar *) s;
}

#if 0
gint
reg_match (gchar *str, gchar *regstr)
{
	regex_t reg;
	gint error;
	gint ret;

	error = regcomp(&reg, regstr, REG_EXTENDED|REG_ICASE|REG_NOSUB);
	if (error != 0) {
		return 0;
	}
	error = regexec(&reg, str, 0, NULL, 0);
	regfree(&reg);
	return (error == 0);
}
#endif

static void
rule_add_subject (RuleContext *context, FilterRule *rule, const gchar *text)
{
	FilterPart *part;
	FilterElement *element;

	/* dont match on empty strings ever */
	if (*text == 0)
		return;
	part = rule_context_create_part (context, "subject");
	filter_rule_add_part ((FilterRule *)rule, part);
	element = filter_part_find_element (part, "subject-type");
	filter_option_set_current ((FilterOption *)element, "contains");
	element = filter_part_find_element (part, "subject");
	filter_input_set_value ((FilterInput *)element, text);
}

static void
rule_add_sender (RuleContext *context, FilterRule *rule, const gchar *text)
{
	FilterPart *part;
	FilterElement *element;

	/* dont match on empty strings ever */
	if (*text == 0)
		return;
	part = rule_context_create_part (context, "sender");
	filter_rule_add_part ((FilterRule *)rule, part);
	element = filter_part_find_element (part, "sender-type");
	filter_option_set_current ((FilterOption *)element, "contains");
	element = filter_part_find_element (part, "sender");
	filter_input_set_value ((FilterInput *)element, text);
}

/* do a bunch of things on the subject to try and detect mailing lists, remove
   unneeded stuff, etc */
static void
rule_match_subject (RuleContext *context, FilterRule *rule, const gchar *subject)
{
	const gchar *s;
	const gchar *s1, *s2;
	gchar *tmp;

	s = strip_re (subject);
	/* dont match on empty subject */
	if (*s == 0)
		return;

	/* [blahblah] is probably a mailing list, match on it separately */
	s1 = strchr (s, '[');
	s2 = strchr (s, ']');
	if (s1 && s2 && s1 < s2) {
		/* probably a mailing list, match on the mailing list name */
		tmp = g_alloca (s2 - s1 + 2);
		memcpy (tmp, s1, s2 - s1 + 1);
		tmp[s2 - s1 + 1] = 0;
		g_strstrip (tmp);
		rule_add_subject (context, rule, tmp);
		s = s2 + 1;
	}
	/* Froblah: at the start is probably something important (e.g. bug number) */
	s1 = strchr (s, ':');
	if (s1) {
		tmp = g_alloca (s1 - s + 1);
		memcpy (tmp, s, s1-s);
		tmp[s1 - s] = 0;
		g_strstrip (tmp);
		rule_add_subject (context, rule, tmp);
		s = s1+1;
	}

	/* just lump the rest together */
	tmp = g_alloca (strlen (s) + 1);
	strcpy (tmp, s);
	g_strstrip (tmp);
	rule_add_subject (context, rule, tmp);
}

static void
rule_match_mlist(RuleContext *context, FilterRule *rule, const gchar *mlist)
{
	FilterPart *part;
	FilterElement *element;

	if (mlist[0] == 0)
		return;

	part = rule_context_create_part(context, "mlist");
	filter_rule_add_part(rule, part);

	element = filter_part_find_element(part, "mlist-type");
	filter_option_set_current((FilterOption *)element, "is");

	element = filter_part_find_element (part, "mlist");
	filter_input_set_value((FilterInput *)element, mlist);
}

static void
rule_from_address (FilterRule *rule, RuleContext *context, CamelInternetAddress* addr, gint flags)
{
	rule->grouping = FILTER_GROUP_ANY;

	if (flags & AUTO_FROM) {
		const gchar *name, *address;
		gchar *namestr;

		camel_internet_address_get (addr, 0, &name, &address);
		rule_add_sender (context, rule, address);
		if (name == NULL || name[0] == '\0')
			name = address;
		namestr = g_strdup_printf(_("Mail from %s"), name);
		filter_rule_set_name (rule, namestr);
		g_free (namestr);
	}
	if (flags & AUTO_TO) {
		rule_match_recipients (context, rule, addr);
	}

}

static void
rule_from_message (FilterRule *rule, RuleContext *context, CamelMimeMessage *msg, gint flags)
{
	CamelInternetAddress *addr;

	rule->grouping = FILTER_GROUP_ANY;

	if (flags & AUTO_SUBJECT) {
		const gchar *subject = msg->subject ? msg->subject : "";
		gchar *namestr;

		rule_match_subject (context, rule, subject);

		namestr = g_strdup_printf (_("Subject is %s"), strip_re (subject));
		filter_rule_set_name (rule, namestr);
		g_free (namestr);
	}
	/* should parse the from address into an internet address? */
	if (flags & AUTO_FROM) {
		const CamelInternetAddress *from;
		gint i;
		const gchar *name, *address;
		gchar *namestr;

		from = camel_mime_message_get_from (msg);
		for (i = 0; from && camel_internet_address_get (from, i, &name, &address); i++) {
			rule_add_sender(context, rule, address);
			if (name == NULL || name[0] == '\0')
				name = address;
			namestr = g_strdup_printf(_("Mail from %s"), name);
			filter_rule_set_name (rule, namestr);
			g_free (namestr);
		}
	}
	if (flags & AUTO_TO) {
		addr = (CamelInternetAddress *)camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_TO);
		if (addr)
			rule_match_recipients (context, rule, addr);
		addr = (CamelInternetAddress *)camel_mime_message_get_recipients (msg, CAMEL_RECIPIENT_TYPE_CC);
		if (addr)
			rule_match_recipients (context, rule, addr);
	}
	if (flags & AUTO_MLIST) {
		gchar *name, *mlist;

		mlist = camel_header_raw_check_mailing_list (&((CamelMimePart *)msg)->headers);
		if (mlist) {
			rule_match_mlist(context, rule, mlist);
			name = g_strdup_printf (_("%s mailing list"), mlist);
			filter_rule_set_name(rule, name);
			g_free(name);
		}
		g_free(mlist);
	}
}

FilterRule *
em_vfolder_rule_from_message (EMVFolderContext *context, CamelMimeMessage *msg, gint flags, const gchar *source)
{
	EMVFolderRule *rule;
	gchar *euri = em_uri_from_camel(source);

	rule = em_vfolder_rule_new ();
	em_vfolder_rule_add_source (rule, euri);
	rule_from_message ((FilterRule *)rule, (RuleContext *)context, msg, flags);
	g_free(euri);

	return (FilterRule *)rule;
}

FilterRule *
em_vfolder_rule_from_address (EMVFolderContext *context, CamelInternetAddress *addr, gint flags, const gchar *source)
{
	EMVFolderRule *rule;
	gchar *euri = em_uri_from_camel(source);

	rule = em_vfolder_rule_new ();
	em_vfolder_rule_add_source (rule, euri);
	rule_from_address ((FilterRule *)rule, (RuleContext *)context, addr, flags);
	g_free(euri);

	return (FilterRule *)rule;
}

FilterRule *
filter_rule_from_message (EMFilterContext *context, CamelMimeMessage *msg, gint flags)
{
	EMFilterRule *rule;
	FilterPart *part;

	rule = em_filter_rule_new ();
	rule_from_message ((FilterRule *)rule, (RuleContext *)context, msg, flags);

	part = em_filter_context_next_action (context, NULL);
	em_filter_rule_add_action (rule, filter_part_clone (part));

	return (FilterRule *)rule;
}

void
filter_gui_add_from_message (CamelMimeMessage *msg, const gchar *source, gint flags)
{
	EMFilterContext *fc;
	gchar *user, *system;
	FilterRule *rule;

	g_return_if_fail (msg != NULL);

	fc = em_filter_context_new ();
	user = g_strdup_printf ("%s/filters.xml",
				mail_component_peek_base_directory (mail_component_peek ()));
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	rule_context_load ((RuleContext *)fc, system, user);
	g_free (system);

	rule = filter_rule_from_message (fc, msg, flags);

	filter_rule_set_source (rule, source);

	rule_context_add_rule_gui ((RuleContext *)fc, rule, _("Add Filter Rule"), user);
	g_free (user);
	g_object_unref (fc);
}

void
mail_filter_rename_uri(CamelStore *store, const gchar *olduri, const gchar *newuri)
{
	EMFilterContext *fc;
	gchar *user, *system;
	GList *changed;
	gchar *eolduri, *enewuri;

	eolduri = em_uri_from_camel(olduri);
	enewuri = em_uri_from_camel(newuri);

	fc = em_filter_context_new ();
	user = g_strdup_printf ("%s/filters.xml", mail_component_peek_base_directory (mail_component_peek ()));
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	rule_context_load ((RuleContext *)fc, system, user);
	g_free (system);

	changed = rule_context_rename_uri((RuleContext *)fc, eolduri, enewuri, g_str_equal);
	if (changed) {
		d(printf("Folder rename '%s' -> '%s' changed filters, resaving\n", olduri, newuri));
		if (rule_context_save((RuleContext *)fc, user) == -1)
			g_warning("Could not write out changed filter rules\n");
		rule_context_free_uri_list((RuleContext *)fc, changed);
	}

	g_free(user);
	g_object_unref(fc);

	g_free(enewuri);
	g_free(eolduri);
}

void
mail_filter_delete_uri(CamelStore *store, const gchar *uri)
{
	EMFilterContext *fc;
	gchar *user, *system;
	GList *deleted;
	gchar *euri;

	euri = em_uri_from_camel(uri);

	fc = em_filter_context_new ();
	user = g_strdup_printf ("%s/filters.xml", mail_component_peek_base_directory (mail_component_peek ()));
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	rule_context_load ((RuleContext *)fc, system, user);
	g_free (system);

	deleted = rule_context_delete_uri ((RuleContext *) fc, euri, g_str_equal);
	if (deleted) {
		GtkWidget *dialog;
		GString *s;
		GList *l;

		s = g_string_new("");
		l = deleted;
		while (l) {
			g_string_append_printf (s, "    %s\n", (gchar *)l->data);
			l = l->next;
		}

		dialog = e_error_new(NULL, "mail:filter-updated", s->str, euri, NULL);
		g_string_free(s, TRUE);
		em_utils_show_info_silent (dialog);

		d(printf("Folder delete/rename '%s' changed filters, resaving\n", euri));
		if (rule_context_save ((RuleContext *) fc, user) == -1)
			g_warning ("Could not write out changed filter rules\n");
		rule_context_free_uri_list ((RuleContext *) fc, deleted);
	}

	g_free(user);
	g_object_unref(fc);
	g_free(euri);
}
