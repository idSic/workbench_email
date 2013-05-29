<?php

/**
 * @file
 * API documentation file for Workbench Email.
 */

/**
 * Allows modules to alter the form select list.
 *
 * If you have some custom logic that falls outside the bounds of
 * the configurable interface, you can alter the form element as
 * needed.
 *
 * hook_form_alter() does not provide a developer with access on
 * ajax events. So this drupal_alter() is provided to gain access
 * to the form element during those ajax events. #kapooya.
 *
 * @param &$form
 *   The form array.
 * @param $email_transition
 *   The email transition occurring. Example, draft to
 *   needs review.
 * @param $user_groups
 *   An associative array of editors and users.
 *   Editors => The workbench access editors. Could be empty.
 *   Users => all the users available under the specified role (or author).
 *
 */
function hook_workbench_email_create_form_element_alter($form, $email_transition, $user_groups) {

}