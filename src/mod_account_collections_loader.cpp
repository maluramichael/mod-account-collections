/*
 * mod-account-collections loader.
 *
 * The playerbots fork auto-globs every module's sources into one lib and looks
 * up a loader symbol derived from the folder name: for folder
 * "mod-account-collections" that symbol is exactly
 * "Addmod_account_collectionsScripts". It must exist and call our real
 * registration function.
 *
 * Released under GNU GPL v2 or (at your option) any later version.
 */

void AddAccountCollectionsScripts();

void Addmod_account_collectionsScripts()
{
    AddAccountCollectionsScripts();
}
