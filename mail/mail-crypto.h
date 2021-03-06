/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef MAIL_CRYPTO_H
#define MAIL_CRYPTO_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus  */

struct _EAccount;

/* PGP/MIME convenience wrappers */
struct _CamelCipherContext *mail_crypto_get_pgp_cipher_context(struct _EAccount *account);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_CRYPTO_H */
