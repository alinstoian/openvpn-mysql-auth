/**
 *
 *  Copyright 2006-2010 Gouverneur Thomas
 *
 *  This file is part of auth-mysql plugin for OpenVPN.
 *
 *  Foobar is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Foobar is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with auth-mysql plugin.  If not, see <http://www.gnu.org/licenses/>.
 *
 **/

/**
 * sqlstuff.c - all the deal with mysql db
 */

#include <mysql/mysql.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sqlstuff.h" /* user, s_conf */


/* 
 * TODO: 
 *        * log mysql error to syslog
 */


struct user * ret_user(struct s_conf *conf, const char *l, const char *p)
{
	MYSQL 		mysql;
	MYSQL_RES 	*result;
	MYSQL_ROW	row;
	char  		*query;
	char			*escaped_login = NULL;
	char			*escaped_passwd = NULL;
	unsigned long escaped_login_length, escaped_passwd_length;
	char			*pass_query = NULL;

	char		*login = (char *) l; /* no warning, be happy */
	struct user	*u;


	mysql_init(&mysql);
	mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "OpenVPN auth-mysql");

	if (!mysql_real_connect(&mysql, conf->hostname, conf->login, conf->passw, conf->db, conf->port, conf->s_path, 0))
		return NULL; /* unable to establish connection */

	/* forge query */

	escaped_login = (char *) malloc	(sizeof(char) * (strlen (l) * 2 + 1));
	escaped_passwd = (char *) malloc (sizeof(char) * (strlen (p) * 2 + 1));
	escaped_login_length = mysql_real_escape_string (&mysql, escaped_login, l, strlen (l));
	escaped_passwd_length = mysql_real_escape_string (&mysql, escaped_passwd, p, strlen (p));
	
	switch (conf->passwd_type)
	{
		case PASSWD_MD5:
			pass_query = (char *) malloc (sizeof(char) * (escaped_passwd_length + 7 + 1)); /* MD5('') */
			sprintf(pass_query, "MD5('%s')", escaped_passwd);
			break;
		case PASSWD_SHA1:
			pass_query = (char *) malloc (sizeof(char) * (escaped_passwd_length + 8 + 1)); /* SHA1('') */
			sprintf(pass_query, "SHA1('%s')", escaped_passwd);
			break;
		default:
			pass_query = (char *) malloc (sizeof(char) * (escaped_passwd_length + 2 + 1)); /* '' */
			sprintf(pass_query, "'%s'", escaped_passwd);
	}
	if (strlen(l) > 50) login[50] = 0; /* Limit of sql field (very bad tricks of *login, uuuugly, but i'm stone so don't blame me ;p */

	query = (char*) malloc(sizeof(char) * 
										( strlen(SQL_GETUSER) 
											+ strlen (conf->id_field) + strlen (conf->login_field) + strlen (conf->passwd_field) 
											+ strlen (conf->table)
											+ strlen (conf->login_field) + escaped_login_length
											+ strlen (conf->passwd_field) + strlen (pass_query)
											+ 1));
	sprintf(query, SQL_GETUSER,
								conf->id_field, conf->login_field, conf->passwd_field,
								conf->table,
								conf->login_field, escaped_login,
								conf->passwd_field, pass_query);

	printf ("Query: %s\n", query);
	free (escaped_login);
	free (escaped_passwd);
	free (pass_query);
	if (mysql_query(&mysql, query))
	{
		free(query);
		mysql_close(&mysql);
		return NULL; /* unable to query mysql */
	}
	free(query);

	if ((result = mysql_store_result(&mysql)) == NULL)
	{
		mysql_close(&mysql);
		return NULL; /* error getting results */
	}

	if (!mysql_num_rows(result))
	{
		mysql_free_result(result);
		mysql_close(&mysql);
		return NULL; /* bad login */
	}

	if ((row = mysql_fetch_row(result)) == NULL)
	{
		mysql_free_result(result);
                mysql_close(&mysql);
		return NULL; /* error getting row */
	}

	u = (struct user *) malloc(sizeof(struct user));

	u->id = atoi(row[0]);

	u->login = (char *) malloc(sizeof(char) * strlen(row[1]) + 1);
	u->passw = (char *) malloc(sizeof(char) * strlen(row[2]) + 1);

	strncpy(u->login, row[1], strlen(row[1]) + 1);
	strncpy(u->passw, row[2], strlen(row[2]) + 1);

	mysql_free_result(result);
	mysql_close(&mysql);

	return u;
}
