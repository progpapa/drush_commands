<?php

/**
 * @file
 * Configures the database to help local development, mostly used after cloning
 * a remote database to the local dev environment.
 *
 * What this command does:
 * - RESETS USERNAME AND PASSWORD FOR USER 1 TO admin / admin
 * - disables a few modules, see _drush_aftersync_disable_modules()
 * - enables a few modules, see _drush_aftersync_enable_modules()
 *   (developer modules and ui modules)
 * - configures variables, see _drush_aftersync_configure_variables()
 *
 * Examples:
 * 1. drush afterlsync
 *    - that's it, no config required
 */


/**
 * Implements hook_drush_command().
 */
function aftersync_drush_command() {
  $items['aftersync'] = array(
    'description' => 'Configures the database to help local development.',
    'examples' => array(
      'drush aftersync' => 'That\'s it, no fancy stuff.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  return $items;
}

/**
* Callback implementation for aftersync command.
*/
function drush_aftersync() {
  // First things first. A non-existent tmp directory might cause trouble later.
  drush_shell_exec("drush variable-set file_temporary_path /tmp");
  drush_log(dt('Temporary directory set to /tmp'), 'success');

  // Generate a list of modules that are already enabled.
  drush_shell_exec('drush pml --pipe --type="module" --status=enabled');
  $enabled_modules = drush_shell_exec_output();
  // Generate a list of modules that are disabled or downloaded but has not yet
  // been installed.
  drush_shell_exec(
      'drush pml --pipe --type="module" --status="disabled,not installed"');
  $disabled_modules = drush_shell_exec_output();
  // Some modules might really annoy you, get rid of them here.
  _drush_aftersync_disable_modules($enabled_modules);
  // Some modules are really needed for local development, make sure they are
  // present and enabled.
  _drush_aftersync_enable_modules($enabled_modules, $disabled_modules);
  // Now set some variable that make life easier.
  _drush_aftersync_configure_variables();
  // Time to make login supereasy.
  _drush_aftersync_reset_user_one();
}

/**
* Implements drush_hook_COMMAND_validate()
*/
function drush_aftersync_validate() {
  // Make sure we are in a Drupal root directory, no aliases allowed.
  // You don't want to accidentaly run this script on a live site using a
  // wrong alias.
  if (empty(drush_sitealias_get_record("@self"))) {
    drush_set_error('AFTERSYNC',
      dt('You are not in a Drupal root directory.'));
    die;
  }
}

/**
 * Some modules are really annoying for development, they are disabled here.
 *
 * @param array $enabled_modules
 *   An array of enabled module names.
 */
function _drush_aftersync_disable_modules($enabled_modules) {
  $disliked_modules = array('overlay', 'toolbar', 'navbar');
  foreach (array_intersect($disliked_modules, $enabled_modules) as $module) {
    drush_shell_exec("drush -y pm-disable $module");
    drush_log(dt("Disabled the $module module."), 'success');

  }
}

/**
 * Some modules are really needed for development, they are enabled here.
 *
 * @param array $enabled_modules
 *   An array of enabled module names.
 * @param array $disabled_modules
 * An array of disabled module names.
 */
function _drush_aftersync_enable_modules($enabled_modules, $disabled_modules) {
  $dev_modules = array(
    'admin_menu',
    'dblog',
    'devel',
    'diff',
    'module_filter',
    'reroute_email',
    'search_krumo',
    'stage_file_proxy',
  );

  // This list also includes submodules that cannot be downloaded
  // but should be enabled.
  $dev_modules_all = $dev_modules;
  $dev_modules_all[] = 'admin_menu_toolbar';

  // Make sure the appropriate UI modules are enabled.
  $ui_modules = array(
    'views' => 'views_ui',
    'rules' => 'rules_admin',
    'context' => 'context_ui',
    'field' => 'field_ui',
  );
  foreach ($ui_modules as $module => $ui_module) {
    if (in_array($module, $enabled_modules)) {
      $dev_modules_all[] = $ui_module;
    }
  }

  // These should be downloaded and enabled.
  $download = array_diff($dev_modules, $enabled_modules, $disabled_modules);
  foreach ($download as $module) {
    drush_shell_exec("drush -y pm-download $module");
    drush_log(dt("Downloaded the $module module."), 'success');
    drush_shell_exec("drush -y pm-enable $module");
    drush_log(dt("Enabled the $module module."), 'success');
  }

  // These should be enabled.
  $enable = array_diff($dev_modules_all, $enabled_modules, $download);
  foreach ($enable as $module) {
    drush_shell_exec("drush -y pm-enable $module");
    drush_log(dt("Enabled the $module module."), 'success');
  }
}

/**
 * Configures a few variables to make development easier.
 */
function _drush_aftersync_configure_variables() {
  // Dev settings for the views module.
  if (module_exists('views_ui')) {
    drush_shell_exec('drush views-dev');
    drush_log(dt('View dev mode configured.'), 'success');
  }

  // Disable caching and css/js aggregation.
  drush_shell_exec("drush variable-set cache FALSE");
  drush_shell_exec("drush variable-set block_cache 0");
  drush_shell_exec("drush variable-set page_compression 0");
  drush_shell_exec("drush variable-set preprocess_css 0");
  drush_shell_exec("drush variable-set preprocess_js 0");
  drush_log(dt('Cache & css,js aggregation turned off.'), 'success');

  // Configure the Reroute Email module.
  $email = 'siteadmin@progpapa.com';
  drush_shell_exec("drush variable-set reroute_email_enable 1");
  drush_shell_exec("drush variable-set reroute_email_address $email");
  drush_shell_exec("drush variable-set reroute_email_enable_message 1");
  drush_log(dt('Email reroute configured, emails are sent to @email',
      array('@email' => $email)), 'success');

  // Disable the AdvAgg module.
  if (module_exists('advagg')) {
    drush_shell_exec("drush variable-set advagg_enabled 0");
    drush_log(dt('Advagg is turned off.'), 'success');
  }

  // Disable the Minify module.
  if (module_exists('minify')) {
    drush_shell_exec("drush variable-set minify_html 0");
    drush_shell_exec("drush variable-set minify_js 0");
    drush_log(dt('Minify is turned off.'), 'success');
  }

  // Show error messages.
  drush_shell_exec("drush variable-set error_level 2");
  drush_log(dt('Error messages are being displayed on the screen.'), 'success');
}

/**
 * Reset the name and password for user 1 to admin/admin.
 */
function _drush_aftersync_reset_user_one() {
  drush_shell_exec('drush sql-query --db-prefix "update {users} set name=\"admin\" where uid = 1"');
  drush_shell_exec('drush user-password --password=admin admin');
  drush_log(dt('Name and password for user 1 was reset.'), 'success');
}