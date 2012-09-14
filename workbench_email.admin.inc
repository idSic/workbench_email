<?php

/**
 * workbench_email_moderation_email_form
 *
 * Form for administering the email content
 */
function workbench_email_form($form, &$form_state){
  $form['#tree'] = TRUE;
  $email_transitions = workbench_email_get_email_transitions();  
  foreach($email_transitions AS $transition_label => $email_transition_set){
    $form[$transition_label] = array(
      '#type' => 'fieldset',
      '#title' => t(ucwords(str_replace('_', ' ', $transition_label))),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
      
    foreach($email_transition_set AS $rid => $email_transition){
      $email = workbench_email_get_email($email_transition, $rid);
      $subject = ''; $message = ''; $send_email = 0;
      if($email){
        //add default values for these fields
        $subject = $email->subject;
        $message = $email->message;        
      }
      $params['rid'] = $rid;
      $params['role'] = $email_transition->role;
      $params['from_state'] = $email_transition->from_name;
      $params['to_state'] = $email_transition->to_name;
          
      $form[$transition_label][$email_transition->role] = array(
        '#type' => 'fieldset',
        '#title' => t('Email ' . ucwords($email_transition->role)),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
          
      $form[$transition_label][$email_transition->role]['subject'] = array(
        '#type' => 'textfield',
        '#default_value' => $subject,
        '#title' => 'Subject',
        '#maxlength' => 255,
      );
            
      $form[$transition_label][$email_transition->role]['message'] = array(
        '#type' => 'textarea',
        '#default_value' => $message,
        '#title' => 'Message',
        '#size' => 30,
      );
    }
  }
  
  $form['token_set'] = array(
    '#type' => 'fieldset',
    '#title' => t('Available Tokens'),
    '#collapsible' => TRUE,
  );  
  
  $form['token_set']['tokens'] = array(
    '#theme' => 'token_tree',
    '#token_types' => array('node','user'), 
    '#global_types' => TRUE, 
    '#click_insert' => TRUE, 
  );
  
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );  
  
  return $form;
}

/**
 * workbench_email_form_validate
 *
 * Validates the form values
 */
function workbench_email_form_validate($form, &$form_state){
  $email_transitions = workbench_email_get_email_transitions();  
  foreach($email_transitions AS $transition_label => $email_transition_set){
    foreach($email_transition_set AS $rid => $email_transition){
      if(isset($form_state['values'][$transition_label])){
        //determine if subject is set with no message
        if($form_state['values'][$transition_label][$email_transition->role]['subject'] != NULL && $form_state['values'][$transition_label][$email_transition->role]['message'] == NULL){
          form_set_error("$transition_label][$email_transition->role][message", 'You must add an email message if a subject is provided');
        }
        //determine if message is set with no subject
        if($form_state['values'][$transition_label][$email_transition->role]['subject'] == NULL && $form_state['values'][$transition_label][$email_transition->role]['message'] != NULL){
          form_set_error("$transition_label][$email_transition->role][subject", 'You must add a subject if an email message is provided');
        }
      }
    }
  }
}

/**
 * workbench_email_form_submit
 *
 * Submit function for the form, saves or deletes information
 */
function workbench_email_form_submit($form, &$form_state){
  $email_transitions = workbench_email_get_email_transitions();  
  foreach($email_transitions AS $transition_label => $email_transition_set){
    foreach($email_transition_set AS $rid => $email_transition){
      $subject = $form_state['values'][$transition_label][$email_transition->role]['subject'];
      $message = $form_state['values'][$transition_label][$email_transition->role]['message'];
      if($subject && $message){  
        workbench_email_save($email_transition, $rid, $subject, $message);
      }
      else{
        workbench_email_delete($email_transition, $rid);      
      }
    }
  }
  drupal_set_message("Email's have been updated");
}

/**
 * Administration form to create and delete email transitions.
 */
function workbench_email_transitions_form($form, &$form_state) {
  $form['transitions'] = array(
    '#tree' => TRUE,
  );

  // List existing states.
  $transitions = workbench_moderation_transitions();

  foreach ($transitions as $transition) {
    $element = array();

    $element['transition'] = array(
      '#type' => 'value',
      '#value' => $transition,
    );
    $element['from_name'] = array(
      '#markup' => check_plain(workbench_moderation_state_label($transition->from_name)),
    );
    $element['to_name'] = array(
      '#markup' => check_plain(workbench_moderation_state_label($transition->to_name)),
    );
    
    $roles = workbench_email_determine_valid_roles('author');
    foreach($roles AS $rid => $role){
      if(strpos($role, 'admin') === FALSE){
        $email_transition = workbench_email_get_email_transitions($transition, $rid);
        $element[$role] = array(
          '#type' => 'checkbox',
          '#title' => t(ucwords($role)),
          '#title_display' => 'invisible',
          '#default_value' => $email_transition ? TRUE : FALSE,
        );
      }
    }

    $form['transitions'][] = $element;
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Transforms the email transitions administration form into a table.
 */
function theme_workbench_email_transitions_form($variables) {
  $form = $variables['form'];
  
  $header = array(
    t('From'),
    '',
    t('To'),
  );
  
  $roles = workbench_email_determine_valid_roles('author');
  $role_names = array();
  foreach($roles AS $rid => $role){
    if(strpos($role, 'admin') === FALSE){
      $role_names[$rid] = ucwords($role);
      $header[] = ucwords($role);
    }
  }  

  $rows = array();
  foreach (element_children($form['transitions']) as $key) {
    $element = &$form['transitions'][$key];
    $row = array('data' => array());
    $row['data']['from'] = drupal_render($element['from_name']);
    $row['data'][] = '--&gt;';
    $row['data']['to'] = drupal_render($element['to_name']);
    foreach($roles AS $rid => $role){
      if(strpos($role, 'admin') === FALSE){
        $row['data'][$role] = drupal_render($element[$role]);
      }
    }
    $rows[] = $row;
  }
  $output = theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('class' => array('width-auto'))));
  $output .= drupal_render_children($form);

  return $output;
}

/**
 * Form submit handler for email transitions.
 */
function workbench_email_transitions_form_submit($form, &$form_state) {
  $roles = workbench_email_determine_valid_roles('author');
  foreach ($form_state['values']['transitions'] as $transition) {
    foreach($roles AS $rid => $role){
      if(strpos($role, 'admin') === FALSE){
        if($transition[$role]){
          workbench_email_transition_save($transition['transition'], $rid, $role);
        }
        else{
          workbench_email_transition_delete($transition['transition'], $rid, $role);
        }
      }
    }
  }
  drupal_set_message('Email transitions have been updated');
}