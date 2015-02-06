
/* crypto/x509/x509_d2.c */

#include <stdio.h>
#include "cryptlib.h"
#include <openssl/crypto.h>
#include <openssl/x509.h>

#ifndef OPENSSL_NO_STDIO
int X509_STORE_set_default_paths(X509_STORE *ctx)
	{
	X509_LOOKUP *lookup;

	lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_file());
	if (lookup == NULL) return(0);
	X509_LOOKUP_load_file(lookup,NULL,X509_FILETYPE_DEFAULT);

	lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_hash_dir());
	if (lookup == NULL) return(0);
	X509_LOOKUP_add_dir(lookup,NULL,X509_FILETYPE_DEFAULT);
	
	/* clear any errors */
	ERR_clear_error();

	return(1);
	}

int X509_STORE_load_locations(X509_STORE *ctx, const char *file,
		const char *path)
	{
	X509_LOOKUP *lookup;

	if (file != NULL)
		{
		lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_file());
		if (lookup == NULL) return(0);
		if (X509_LOOKUP_load_file(lookup,file,X509_FILETYPE_PEM) != 1)
		    return(0);
		}
	if (path != NULL)
		{
		lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_hash_dir());
		if (lookup == NULL) return(0);
		if (X509_LOOKUP_add_dir(lookup,path,X509_FILETYPE_PEM) != 1)
		    return(0);
		}
	if ((path == NULL) && (file == NULL))
		return(0);
	return(1);
	}
//!!== Chiwei [2010/10/30]: Android AGPS porting ==
/* add by Will for DER format certificate pool */ //add-->
int X509_STORE_set_default_paths_ext(X509_STORE *ctx, int filetype)
	{
	X509_LOOKUP *lookup;

    if (filetype != X509_FILETYPE_ASN1)
        return X509_STORE_set_default_paths(ctx);
       
	lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_file());
	if (lookup == NULL) return(0);
	X509_LOOKUP_load_file(lookup,NULL,X509_FILETYPE_ASN1);

	lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_hash_dir());
	if (lookup == NULL) return(0);
	X509_LOOKUP_add_dir(lookup,NULL,X509_FILETYPE_ASN1);
	
	/* clear any errors */
	ERR_clear_error();
	return(1);
	}

int X509_STORE_load_locations_ext(X509_STORE *ctx, const char *file,
		const char *path, int filetype)
	{
	X509_LOOKUP *lookup;

    if (filetype != X509_FILETYPE_ASN1)
        return X509_STORE_load_locations(ctx, file, path);

	if (file != NULL)
		{
		lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_file());
		if (lookup == NULL) return(0);
		if (X509_LOOKUP_load_file(lookup,file,X509_FILETYPE_ASN1) != 1)
		    return(0);
		}
	if (path != NULL)
		{
		lookup=X509_STORE_add_lookup(ctx,X509_LOOKUP_hash_dir());
		if (lookup == NULL) return(0);
		if (X509_LOOKUP_add_dir(lookup,path,X509_FILETYPE_ASN1) != 1)
		    return(0);
		}
	if ((path == NULL) && (file == NULL))
		return(0);
	return(1);
	}
/* add by Will for DER format certificate pool */ //add<--
#endif
