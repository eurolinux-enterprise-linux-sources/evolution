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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <camel/camel-gpg-context.h>
#include <libedataserver/e-account.h>

#include "mail-crypto.h"
#include "mail-session.h"

/**
 * mail_crypto_get_pgp_cipher_context:
 * @account: Account that will be using this context
 *
 * Constructs a new GnuPG cipher context with the appropriate
 * options set based on the account provided.
 **/
CamelCipherContext *
mail_crypto_get_pgp_cipher_context (EAccount *account)
{
	CamelCipherContext *cipher;

	cipher = camel_gpg_context_new (session);
	if (account)
		camel_gpg_context_set_always_trust ((CamelGpgContext *) cipher, account->pgp_always_trust);

	return cipher;
}
