#include <stdio.h>
#include <stdlib.h>

#include <opkg_conf.h>
#include <opkg_cmd.h>
#include <opkg_message.h>
#include <opkg_download.h>
#include <opkg_upgrade.h>
#include <opkg_configure.h>
#include <opkg_install.h>
#include <opkg_remove.h>
#include <file_util.h>
#include <release.h>
#include <pkg.h>
#include <xsystem.h>
#include <xfuncs.h>
#include <sprintf_alloc.h>

#include <signal.h>     // signal
#include <sys/types.h>  // opendir
#include <dirent.h>     // opendir

#include <fnmatch.h>    // fnmatch

#include "util.h"
#include "log.h"

static int
_opkg_update()
{
    RET_IF(!opkg_config->tmp_dir, false);

    if (!_file_is_dir(opkg_config->lists_dir)) {
        if (_file_exist(opkg_config->lists_dir)) {
            ERR("%s is not a directory", opkg_config->lists_dir);
            return false;
        } else {
            if (!_file_mkdir(opkg_config->lists_dir, 0755)) {
                ERR("file mkdir hier failed: %s", opkg_config->lists_dir);
                return  false;
            }
        }
    }
    char *tmp, *dtmp;

    int err;
    int failures = 0;
    tmp = _strdup_printf("%s/update-XXXXXX", opkg_config->tmp_dir);
    dtmp = mkdtemp(tmp);
    if (!dtmp) {
        ERR("mkdtemp failed (%s): %s", tmp, dtmp);
        free(tmp);
        return false;
    }

    pkg_src_list_elt_t *iter;
    for (iter = void_list_first(&opkg_config->dist_src_list); iter;
            iter = void_list_next(&opkg_config->dist_src_list, iter)) {
        char *url, *list_file_name;

        pkg_src_t *src = (pkg_src_t *) iter->data;

        url = _strdup_printf("%s/dists/%s/Release", src->value, src->name);
        list_file_name = _strdup_printf("%s/%s", opkg_config->lists_dir, src->name);
        LOG("url: %s", url);

        err = opkg_download(url, list_file_name, NULL, NULL);
        if (!err) {
            release_t *release = release_new();
            err = release_init_from_file(release, list_file_name);
            if (!err) {
                if (!release_comps_supported(release, src->extra_data))
                    err = -1;
            }
            if (!err) {
                err = release_download(release, src, opkg_config->lists_dir, tmp);
            }
            release_deinit(release);
            if (err) unlink(list_file_name);
        }

        if (err) failures++;

        free(list_file_name);
        free(url);
    }

    for (iter = void_list_first(&opkg_config->pkg_src_list); iter;
            iter = void_list_next(&opkg_config->pkg_src_list, iter)) {
        pkg_src_t *src = (pkg_src_t *) iter->data;

        LOG("%s %s %s", src->name, src->value, src->extra_data);
        if (src->extra_data && !strcmp(src->extra_data, "__dummy__ "))
            continue;

        // blocking function
        err = pkg_src_update(src);
        if (err) failures++;
    }
    rmdir(tmp);
    free(tmp);

    return failures;
}

static bool
_opkg_list()
{
    pkg_vec_t *available = pkg_vec_alloc();
    pkg_hash_fetch_available(available);
    pkg_vec_sort(available, pkg_compare_names);

    for (unsigned int i = 0; i < available->len ; i++) {
        pkg_t *pkg = available->pkgs[i];
        printf("pkg->name: %s\n", pkg->name);
    }
    pkg_vec_free(available);
    return true;
}

static bool
_opkg_list_installed()
{
    pkg_vec_t *available = pkg_vec_alloc();
    pkg_hash_fetch_all_installed(available);
    pkg_vec_sort(available, pkg_compare_names);
    LOG("%d", available->len);
    for (unsigned int i = 0; i < available->len ; i++) {
        pkg_t *pkg = available->pkgs[i];
        printf("pkg->name: %s\n", pkg->name);
    }
    pkg_vec_free(available);
    return true;
}

typedef struct _Pkg_Map {
    int state;
    char *str;
} Pkg_Map;

const Pkg_Map _pkg_state_want_map[5] = {
    {SW_UNKNOWN, "unknown"},
    {SW_INSTALL, "install"},
    {SW_DEINSTALL, "deinstall"},
    {SW_PURGE, "purge"},
};

const Pkg_Map _pkg_state_flag_map[] = {
    {SF_OK, "ok"},
    {SF_REINSTREQ, "reinstreq"},
    {SF_HOLD, "hold"},
    {SF_REPLACE, "replace"},
    {SF_NOPRUNE, "noprune"},
    {SF_PREFER, "prefer"},
    {SF_OBSOLETE, "obsolete"},
    {SF_USER, "user"},
};

const Pkg_Map _pkg_state_status_map[] = {
    {SS_NOT_INSTALLED, "not-installed"},
    {SS_UNPACKED, "unpacked"},
    {SS_HALF_CONFIGURED, "half-configured"},
    {SS_INSTALLED, "installed"},
    {SS_HALF_INSTALLED, "half-installed"},
    {SS_CONFIG_FILES, "config-files"},
    {SS_POST_INST_FAILED, "post-inst-failed"},
    {SS_REMOVAL_FAILED, "removal-failed"},
};

static const char *
_pkg_state_flag_to_str(pkg_state_flag_t state)
{
    unsigned int len = sizeof(_pkg_state_flag_map);
    for (unsigned int i = 0 ; i < len ; i++) {
        if (state == _pkg_state_flag_map[i].state) {
            return _pkg_state_flag_map[i].str;
        }
    }
    return NULL;
}

static const char *
_pkg_state_want_to_str(pkg_state_want_t state)
{
    unsigned int len = sizeof(_pkg_state_want_map);
    for (unsigned int i = 0 ; i < len ; i++) {
        if (state == _pkg_state_want_map[i].state) {
            return _pkg_state_want_map[i].str;
        }
    }
    return NULL;
}

static const char *
_pkg_state_status_to_str(pkg_state_status_t state)
{
    unsigned int len = sizeof(_pkg_state_status_map);
    for (unsigned int i = 0 ; i < len ; i++) {
        if (state == _pkg_state_status_map[i].state) {
            return _pkg_state_status_map[i].str;
        }
    }
    return NULL;
}

static void
_opkg_print_pkg(pkg_t *pkg)
{
    RET_IF(!pkg);
    unsigned int i, j;
    LOG("Package: %s", pkg->name);
    if (pkg->priority) LOG("Priority: %s", pkg->priority);
    if (pkg->section) LOG("Section: %s", pkg->section);
    if (pkg->installed_size) LOG("Installed-Size: %ld", pkg->installed_size);
    if (pkg->installed_time) LOG("Installed-Time: %lu", pkg->installed_time);
    if (pkg->auto_installed) LOG("Auto-Installed: yes");
    if (pkg->tags) LOG("Tags: %s", pkg->tags);
    if (pkg->maintainer) LOG("Maintainer: %s", pkg->maintainer);
    if (pkg->architecture) LOG("Architecture: %s", pkg->architecture);

    char *version = pkg_version_str_alloc(pkg);
    if (version) {
        LOG("Version: %s", version);
        free(version);
    }
    unsigned int depends_count = pkg->pre_depends_count + pkg->depends_count
        + pkg->recommends_count + pkg->suggests_count;
    if (pkg->depends_count) {
        LOG("Depends:");
        for (j = 0, i = 0; i < depends_count; i++) {
            if (pkg->depends[i].type != DEPEND) continue;
            char *str = pkg_depend_str(pkg, i);
            LOG("%s %s", j == 0 ? "" : ",", str);
            free(str);
            j++;
        }
    }
    if (pkg->recommends_count) {
        LOG("Recommends:");
        for (j = 0, i = 0; i < depends_count; i++) {
            if (pkg->depends[i].type != RECOMMEND)
                continue;
            char *str = pkg_depend_str(pkg, i);
            LOG("%s %s", j == 0 ? "" : ",", str);
            free(str);
            j++;
        }
    }
    if (pkg->suggests_count) {
        LOG("Suggests:");
        for (j = 0, i = 0; i < depends_count; i++) {
            if (pkg->depends[i].type != SUGGEST) continue;
            char *str = pkg_depend_str(pkg, i);
            LOG("%s %s", j == 0 ? "" : ",", str);
            free(str);
            j++;
        }
    }
    if (pkg->provides_count > 1) {
        LOG("Provides:");
        for (i = 1; i < pkg->provides_count; i++) {
            LOG("%s %s", i == 1 ? "" : ",",
                    pkg->provides[i]->name);
        }
    }
    if (pkg->replaces_count) {
        LOG("Replaces:");
        for (i = 0; i < pkg->replaces_count; i++) {
            LOG("%s %s", i == 0 ? "" : ",",
                    pkg->replaces[i]->name);
        }
    }
    struct depend *cdep;
    if (pkg->conflicts_count) {
        LOG("Conflicts:");
        for (i = 0; i < pkg->conflicts_count; i++) {
            cdep = pkg->conflicts[i].possibilities[0];
            LOG("%s %s", i == 0 ? "" : ",", cdep->pkg->name);
            if (cdep->version) {
                LOG(" (%s%s)",
                        constraint_to_str(cdep->constraint),
                        cdep->version);
            }
        }
    }
    LOG("Status: %s %s %s",
            _pkg_state_want_to_str(pkg->state_want),
            _pkg_state_flag_to_str(pkg->state_flag),
            _pkg_state_status_to_str(pkg->state_status));

    if (pkg->essential) LOG("Essential: yes");
    conffile_list_elt_t *iter;
    if (!nv_pair_list_empty(&pkg->conffiles)) {
        // confflies
        LOG("Conffiles:");
        for (iter = nv_pair_list_first(&pkg->conffiles); iter;
                iter = nv_pair_list_next(&pkg->conffiles, iter)) {
            conffile_t * cf = (conffile_t *) iter->data;
            if (cf->name && cf->value) {
                LOG(" %s %s", ((conffile_t *) iter->data)->name,
                        ((conffile_t *) iter->data)->value);
            }
        }
    }
    if (pkg->filename) LOG("Filename: %s", pkg->filename);
    if (pkg->size) LOG("Size: %ld", pkg->size);
    if (pkg->md5sum) LOG("MD5Sum: %s", pkg->md5sum);
    if (pkg->sha256sum) LOG("MD5Sum: %s", pkg->sha256sum);
    if (pkg->description) LOG("Description: %s", pkg->description);
}

static bool
_opkg_status(const char *pkg_name, int installed)
{
    unsigned int i, err;
    pkg_vec_t *available;
    pkg_t *pkg;
    char b_match = 0;

    available = pkg_vec_alloc();
    if (installed) pkg_hash_fetch_all_installed(available);
    else pkg_hash_fetch_available(available);

    LOG("Avaailable %d", available->len);
    for (i = 0; i < available->len; i++) {
        pkg = available->pkgs[i];
        if (pkg_name && fnmatch(pkg_name, pkg->name, 0)) continue;

        _opkg_print_pkg(pkg);

        b_match = 1;
    }
    pkg_vec_free(available);

    if (!b_match && pkg_name && file_exists(pkg_name)) {
        pkg = pkg_new();
        err = pkg_init_from_file(pkg, pkg_name);
        if (err)
            return err;
        hash_insert_pkg(pkg, 0);
        pkg_formatted_info(stdout, pkg);
    }

    return 0;
}

static void
_opkg_search(const char *file_name)
{
    RET_IF(!file_name);
    unsigned int i;

    pkg_vec_t *installed;
    pkg_t *pkg;
    str_list_t *installed_files;
    str_list_elt_t *iter;
    char *installed_file;

    installed = pkg_vec_alloc();
    pkg_hash_fetch_all_installed(installed);
    pkg_vec_sort(installed, pkg_compare_names);

    for (i = 0; i < installed->len; i++) {
        pkg = installed->pkgs[i];

        installed_files = pkg_get_installed_files(pkg);

        for (iter = str_list_first(installed_files); iter;
                iter = str_list_next(installed_files, iter)) {
            installed_file = (char *)iter->data;
            if (fnmatch(file_name, installed_file, 0) == 0) {
                char *version = pkg_version_str_alloc(pkg);
                LOG("%s - %s - %s", pkg->name, version, pkg->description);
                free(version);
            }
        }
        pkg_free_installed_files(pkg);
    }
    pkg_vec_free(installed);
}

static void
_opkg_files(const char *pkg_name)
{
    RET_IF(!pkg_name);
    pkg_t *pkg;
    str_list_t *files;
    str_list_elt_t *iter;
    char *pkg_version;

    pkg = pkg_hash_fetch_installed_by_name(pkg_name);
    if (!pkg) {
        ERR("Package %s is not installed", pkg_name);
        return;
    }

    files = pkg_get_installed_files(pkg);
    pkg_version = pkg_version_str_alloc(pkg);

    LOG("Package %s (%s) is installed on %s and has the following files: ",
            pkg->name, pkg_version, pkg->dest->name);

    for (iter = str_list_first(files); iter; iter = str_list_next(files, iter))
        LOG("%s", (char *)iter->data);

    free(pkg_version);
    pkg_free_installed_files(pkg);
}

struct opkg_intercept {
    char *oldpath;
    char *statedir;
};

typedef struct opkg_intercept *opkg_intercept_t;

static int
opkg_finalize_intercepts(opkg_intercept_t ctx)
{
    DIR *dir;
    int err = 0;

    setenv("PATH", ctx->oldpath, 1);
    free(ctx->oldpath);

    dir = opendir(ctx->statedir);
    if (dir) {
        struct dirent *de;
        while (de = readdir(dir), de != NULL) {
            char *path;

            if (de->d_name[0] == '.')
                continue;

            path = _strdup_printf("%s/%s", ctx->statedir, de->d_name);
            if (access(path, X_OK) == 0) {
                const char *argv[] = { "sh", "-c", path, NULL };
                xsystem(argv);
            }
            free(path);
        }
        closedir(dir);
    } else
        ERR("Failed to open dir %s", ctx->statedir);

    rm_r(ctx->statedir);
    free(ctx->statedir);
    free(ctx);

    return err;
}

/* For package pkg do the following: If it is already visited, return. If not,
   add it in visited list and recurse to its deps. Finally, add it to ordered
   list.
   pkg_vec all contains all available packages in repos.
   pkg_vec visited contains packages already visited by this function, and is
   used to end recursion and avoid an infinite loop on graph cycles.
   pkg_vec ordered will finally contain the ordered set of packages.
*/
static int opkg_recurse_pkgs_in_order(pkg_t * pkg, pkg_vec_t * all,
                                      pkg_vec_t * visited, pkg_vec_t * ordered)
{
    int j, k;
    unsigned int i, l, m;
    int count;
    pkg_t *dep;
    compound_depend_t *compound_depend;
    depend_t **possible_satisfiers;
    abstract_pkg_t *abpkg;
    abstract_pkg_t **dependents;

    /* If it's just an available package, that is, not installed and not even
     * unpacked, skip it */
    /* XXX: This is probably an overkill, since a state_status != SS_UNPACKED
     * would do here. However, if there is an intermediate node (pkg) that is
     * configured and installed between two unpacked packages, the latter
     * won't be properly reordered, unless all installed/unpacked pkgs are
     * checked */
    if (pkg->state_status == SS_NOT_INSTALLED)
        return 0;

    /* If the  package has already been visited (by this function), skip it */
    for (i = 0; i < visited->len; i++)
        if (!strcmp(visited->pkgs[i]->name, pkg->name)) {
            opkg_msg(DEBUG, "pkg %s already visited, skipping.\n", pkg->name);
            return 0;
        }

    pkg_vec_insert(visited, pkg);

    count = pkg->pre_depends_count + pkg->depends_count +
        pkg->recommends_count + pkg->suggests_count;

    opkg_msg(DEBUG, "pkg %s.\n", pkg->name);

    /* Iterate over all the dependencies of pkg. For each one, find a package
     * that is either installed or unpacked and satisfies this dependency.
     * (there should only be one such package per dependency installed or
     * unpacked). Then recurse to the dependency package */
    for (j = 0; j < count; j++) {
        compound_depend = &pkg->depends[j];
        possible_satisfiers = compound_depend->possibilities;
        for (k = 0; k < compound_depend->possibility_count; k++) {
            abpkg = possible_satisfiers[k]->pkg;
            dependents = abpkg->provided_by->pkgs;
            l = 0;
            if (dependents != NULL)
                while (l < abpkg->provided_by->len && dependents[l] != NULL) {
                    opkg_msg(DEBUG, "Descending on pkg %s.\n",
                             dependents[l]->name);

                    /* find whether dependent l is installed or unpacked,
                     * and then find which package in the list satisfies it */
                    for (m = 0; m < all->len; m++) {
                        dep = all->pkgs[m];
                        if (dep->state_status != SS_NOT_INSTALLED)
                            if (!strcmp(dep->name, dependents[l]->name)) {
                                opkg_recurse_pkgs_in_order(dep, all, visited,
                                                           ordered);
                                /* Stop the outer loop */
                                l = abpkg->provided_by->len;
                                /* break from the inner loop */
                                break;
                            }
                    }
                    l++;
                }
        }
    }

    /* When all recursions from this node down, are over, and all
     * dependencies have been added in proper order in the ordered array, add
     * also the package pkg to ordered array */
    pkg_vec_insert(ordered, pkg);

    return 0;

}

#define DATADIR "/usr/local/share"
static opkg_intercept_t opkg_prep_intercepts(void)
{
    opkg_intercept_t ctx;
    char *newpath;
    char *dtemp;

    ctx = xcalloc(1, sizeof(*ctx));
    ctx->oldpath = xstrdup(getenv("PATH"));
    sprintf_alloc(&newpath, "%s/opkg/intercept:%s", DATADIR, ctx->oldpath);
    sprintf_alloc(&ctx->statedir, "%s/opkg-intercept-XXXXXX",
                  opkg_config->tmp_dir);

    dtemp = mkdtemp(ctx->statedir);
    if (dtemp == NULL) {
        opkg_perror(ERROR, "Failed to make temp dir %s", ctx->statedir);
        free(ctx->oldpath);
        free(ctx->statedir);
        free(newpath);
        free(ctx);
        return NULL;
    }

    setenv("OPKG_INTERCEPT_DIR", ctx->statedir, 1);
    setenv("PATH", newpath, 1);
    free(newpath);

    return ctx;
}

static int
opkg_configure_packages(char *pkg_name)
{
    pkg_vec_t *all, *ordered, *visited;
    unsigned int i;
    pkg_t *pkg;
    opkg_intercept_t ic;
    int r, err = 0;

    if (opkg_config->offline_root && !opkg_config->force_postinstall) {
        opkg_msg(INFO,
                 "Offline root mode: not configuring unpacked packages.\n");
        return 0;
    }
    opkg_msg(INFO, "Configuring unpacked packages.\n");

    all = pkg_vec_alloc();

    pkg_hash_fetch_available(all);

    /* Reorder pkgs in order to be configured according to the Depends: tag
     * order */
    opkg_msg(INFO, "Reordering packages before configuring them...\n");
    ordered = pkg_vec_alloc();
    visited = pkg_vec_alloc();
    for (i = 0; i < all->len; i++) {
        pkg = all->pkgs[i];
        opkg_recurse_pkgs_in_order(pkg, all, visited, ordered);
    }

    ic = opkg_prep_intercepts();
    if (ic == NULL) {
        err = -1;
        goto error;
    }

    for (i = 0; i < ordered->len; i++) {
        pkg = ordered->pkgs[i];

        if (pkg_name && fnmatch(pkg_name, pkg->name, 0))
            continue;

        if (pkg->state_status == SS_UNPACKED) {
            opkg_msg(NOTICE, "Configuring %s.\n", pkg->name);
            r = opkg_configure(pkg);
            if (r == 0) {
                pkg->state_status = SS_INSTALLED;
                pkg->parent->state_status = SS_INSTALLED;
                pkg->state_flag &= ~SF_PREFER;
                opkg_state_changed++;
            } else {
                if (!opkg_config->offline_root)
                    err = -1;
            }
        }
    }

    r = opkg_finalize_intercepts(ic);
    if (r != 0)
        err = -1;

 error:
    pkg_vec_free(all);
    pkg_vec_free(ordered);
    pkg_vec_free(visited);

    return err;
}

static void
write_status_files_if_changed(void)
{
    if (opkg_state_changed && !opkg_config->noaction) {
        opkg_msg(INFO, "Writing status file.\n");
        opkg_conf_write_status_files();
        pkg_write_changed_filelists();
        if (!opkg_config->offline_root)
            sync();
    } else {
        opkg_msg(DEBUG, "Nothing to be done.\n");
    }
}

static void sigint_handler(int sig)
{
    signal(sig, SIG_DFL);
    opkg_msg(NOTICE, "Interrupted. Writing out status database.\n");
    write_status_files_if_changed();
    exit(128 + sig);
}

static int
_opkg_remove(int argc, char **argv)
{
    int i, done, err = 0;
    unsigned int a;
    pkg_t *pkg;
    pkg_t *pkg_to_remove;
    pkg_vec_t *available;
    int r;

    done = 0;

    signal(SIGINT, sigint_handler);

    pkg_info_preinstall_check();

    available = pkg_vec_alloc();
    pkg_hash_fetch_all_installed(available);

    for (i = 0; i < argc; i++) {
        for (a = 0; a < available->len; a++) {
            pkg = available->pkgs[a];
            if (fnmatch(argv[i], pkg->name, 0)) {
                continue;
            }
            if (opkg_config->restrict_to_default_dest) {
                pkg_to_remove = pkg_hash_fetch_installed_by_name_dest(pkg->name,
                        opkg_config->default_dest);
            } else {
                pkg_to_remove = pkg_hash_fetch_installed_by_name(pkg->name);
            }

            if (pkg_to_remove == NULL) {
                opkg_msg(ERROR, "Package %s is not installed.\n", pkg->name);
                continue;
            }
            if (pkg->state_status == SS_NOT_INSTALLED) {
                opkg_msg(ERROR, "Package %s not installed.\n", pkg->name);
                continue;
            }

            r = opkg_remove_pkg(pkg_to_remove);
            if (r != 0)
                err = -1;
            else
                done = 1;
        }
    }

    pkg_vec_free(available);

    if (done == 0)
        opkg_msg(NOTICE, "No packages removed.\n");

    write_status_files_if_changed();
    return err;
}
static int
_opkg_install(int argc, char **argv)
{
    int i;
    char *arg;
    int err = 0;
    str_list_t *pkg_names_to_install = NULL;
    int r;

    signal(SIGINT, sigint_handler);

    /*
     * Now scan through package names and install
     */
    for (i = 0; i < argc; i++) {
        arg = argv[i];

        opkg_msg(DEBUG2, "%s\n", arg);
        r = opkg_prepare_url_for_install(arg, &argv[i]);
        if (r != 0)
            return -1;
    }
    pkg_info_preinstall_check();

    if (opkg_config->combine)
        pkg_names_to_install = str_list_alloc();

    for (i = 0; i < argc; i++) {
        arg = argv[i];
        if (opkg_config->combine) {
            str_list_append(pkg_names_to_install, arg);
        } else {
            r = opkg_install_by_name(arg);
            if (r != 0) {
                opkg_msg(ERROR, "Cannot install package %s.\n", arg);
                err = -1;
            }
        }
    }

    if (opkg_config->combine) {
        r = opkg_install_multiple_by_name(pkg_names_to_install);
        if (r != 0)
            err = -1;

        str_list_purge(pkg_names_to_install);
    }

    r = opkg_configure_packages(NULL);
    if (r != 0)
        err = -1;

    write_status_files_if_changed();

    return err;
}

static void
_opkg_upgrade(int cnt, char **pkg_name)
{
    int i;
    unsigned int j;
    pkg_t *pkg;
    int err = 0;
    pkg_vec_t *pkgs_to_upgrade = NULL;
    int r;

    signal(SIGINT, sigint_handler);

    pkg_info_preinstall_check();

    if (opkg_config->combine) pkgs_to_upgrade = pkg_vec_alloc();

    if (cnt) {
        for (i = 0; i < cnt; i++) {
            if (opkg_config->restrict_to_default_dest) {
                pkg = pkg_hash_fetch_installed_by_name_dest(pkg_name[i],
                        opkg_config->default_dest);
                if (!pkg) {
                    LOG("Package %s not installed in %s.",
                            pkg_name[i], opkg_config->default_dest->name);
                    continue;
                }
            } else {
                pkg = pkg_hash_fetch_installed_by_name(pkg_name[i]);
                if (!pkg) {
                    LOG("Package %s not installed.", pkg_name[i]);
                    continue;
                }
            }
            if (opkg_config->combine) {
                pkg_vec_insert(pkgs_to_upgrade, pkg);
            } else {
                r = opkg_upgrade_pkg(pkg);
                if (r != 0) err = -1;
            }
        }

        if (opkg_config->combine) {
            r = opkg_upgrade_multiple_pkgs(pkgs_to_upgrade);
            if (r != 0) err = -1;

            pkg_vec_free(pkgs_to_upgrade);
        }
    } else {
        pkg_vec_t *installed = pkg_vec_alloc();

        pkg_info_preinstall_check();

        pkg_hash_fetch_all_installed(installed);

        if (opkg_config->combine) {
            err = opkg_upgrade_multiple_pkgs(installed);
        } else {
            for (j = 0; j < installed->len; j++) {
                pkg = installed->pkgs[j];
                r = opkg_upgrade_pkg(pkg);
                if (r != 0) err = -1;
            }
        }
        pkg_vec_free(installed);
    }

    r = opkg_configure_packages(NULL);
    if (r != 0) err = -1;

    write_status_files_if_changed();
}

int main(int argc, char *argv[])
{
    RET_IF(argc < 2, -1);
    RET_IF(!argv || !argv[0] || !argv[1], -1);

    const char *cmd_name = argv[1];
    const char *name = NULL;
    if (argc == 3 && argv[2]) name = argv[2];

    if (opkg_conf_init()) {
        ERR("pkg conf init failed");
        return -1;
    }
    opkg_config->verbosity = INFO; // NOTICE

    if (opkg_conf_load()) {
        ERR("opkg conf load failed");
        return -1;
    }
    printf("lists dir: %s\n", opkg_config->lists_dir);

    pkg_hash_load_feeds();
    pkg_hash_load_status_files();

    if (!strcmp("update", cmd_name)) _opkg_update();
    else if (!strcmp("status", cmd_name)) _opkg_status(name, false);
    else if (!strcmp("list", cmd_name)) _opkg_list();
    else if (!strcmp("list-installed", cmd_name)) _opkg_list_installed();
    else if (!strcmp("search", cmd_name)) _opkg_search(name);
    else if (!strcmp("files", cmd_name)) _opkg_files(name);
    else if (!strcmp("upgrade", cmd_name)) _opkg_upgrade(argc - 2, argv+2);
    else if (!strcmp("install", cmd_name)) _opkg_install(argc - 2, argv+2);
    else if (!strcmp("remove", cmd_name)) _opkg_remove(argc - 2, argv+2);

    opkg_download_cleanup();

    opkg_conf_deinit();

    return 0;
}

