<?php

commerce_promotion_balalaika_autoloader();

/**
 * Page callback: build a form for promotion.
 */
function commerce_promotion_get_form($promotion_id = NULL) {
  $promotion = $promotion_id ? 'something' : commerce_promotion_defaults();
  $form_state['build_info']['args'] = array($promotion);

  return drupal_build_form('commerce_promotion_form', $form_state);
}

/**
 * Form callback: create or edit a promotion.
 */
function commerce_promotion_form($form, &$form_state, $promotion) {
  $form_state['promotion'] = $promotion;

  // This field is required but let's validate it manually so that
  // AJAX interactions are less restrictive (#limit_validation_errors, etc.).
  $form['#attributes'] = array('class' => array('commerce-promotion-form'));

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Promotion Name') . ' <span class="form-required" title="This field is required.">*</span>',
    '#description' => t('Shown to customers.'),
    '#default_value' => $promotion['name'],
  );

  $form['status'] = array(
    '#type' => 'radios',
    '#title' => t('Status'),
    '#options' => array(1 => t('Enabled'), 0 => t('Disabled')),
    '#default_value' => $promotion['status'],
  );

  $form['activators'] = array(
    '#type' => 'fieldset',
    '#attributes' => array('id' => 'promotion-activators'),
    '#title' => t('Activate when:'),
  );

  commerce_promotion_set_codes_form($form, $form_state, $promotion);

  commerce_promotion_set_date_range_form($form, $form_state, $promotion);

  commerce_promotion_multiple_items_form($form, $form_state, 'rules',
    array('title' => 'Conditions required to apply it', 'add_button' => 'Add Condition'));

  commerce_promotion_multiple_items_form($form, $form_state, 'actions',
    array('title' => 'Actions to perform', 'add_button' => 'Add Action'));

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create Promotion'),
  );

  return $form;
}

/**
 * Form builder for AJAX forms that can have many different field groups.
 */
function commerce_promotion_multiple_items_form(&$form, &$form_state, $base_name, $display) {
  $add_button_name = $base_name . '_add_button';

  $form[$base_name] = array(
    '#type' => 'fieldset',
    '#title' => $display['title'],
    '#attributes' => array('id' => $base_name . '-fs'),
  );

  $builder = commerce_promotion_get_builder();
  $builder_name = 'get' . ucfirst($base_name);
  $config = $builder->$builder_name();
  $form_state[$base_name . '_config'] = $config;

  $form[$base_name][$base_name . '_options'] = array(
    '#prefix' => '<div class="container-inline">',
    '#type' => 'select',
    '#options' => commerce_promotion_get_component_options($config),
    '#default_option' => '',
  );

  $form[$base_name][$base_name . '_add'] = array(
    '#type' => 'button',
    '#value' => $display['add_button'],
    '#name' => $add_button_name,
    '#ajax' => array(
      'callback' => 'commerce_promotion_ajax_'. $base_name .'_refresh',
      'wrapper' => 'promotion-' . $base_name,
    ),
    '#suffix' => '</div>',
  );

  $form[$base_name][$base_name . '_values'] = array(
    '#type' => 'container',
    '#tree' => TRUE,
    '#prefix' => '<div id="promotion-'. $base_name .'">',
    '#suffix' => '</div>',
  );

  if (isset($form_state['triggering_element'])) {
    if (strpos($form_state['triggering_element']['#name'], $base_name . '-remove') !== FALSE) {
      $id = str_replace($base_name . '-remove-', '', $form_state['triggering_element']['#name']);
      unset($form_state[$base_name . '_values'][$id]);
    }

    if ($form_state['triggering_element']['#name'] == $add_button_name) {
      if (!isset($form_state[$base_name . '_count'])) {
        $form_state[$base_name . '_count'] = 0;
        $form_state[$base_name . '_values'] = array();
      }

      $rule_form_container = array(
        '#prefix' => '<div class="container-inline promotion-property">',
        '#suffix' => '</div>',
      );

      $rule_form = commerce_promotion_get_component_form($base_name, $form_state);
      foreach ($rule_form as $name => $field) {
        $rule_form_container[$name] = $field;
      }

      $form_state[$base_name . '_values'][$form_state[$base_name . '_count']] = $rule_form_container;
      $form_state[$base_name . '_count']++;
    }
  }

  // If there are rules values in the state.
  if (isset($form_state[$base_name . '_count'])) {
    // Render all rules.
    for ($i = 0; $i <= $form_state[$base_name . '_count']; $i++) {
      if (isset($form_state[$base_name . '_values'][$i])) {
        foreach ($form_state[$base_name . '_values'][$i] as $key => $field) {
          $form[$base_name][$base_name . '_values'][$i][$key] = $field;
        }
      }
    }
  }
}

/**
 * Return available rules/actions as options for a select field.
 */
function commerce_promotion_get_component_options($config) {
  $options = array();
  foreach ($config as $key => $config) {
    $options[$key] = $config['name'];
  }

  return $options;
}

/**
 * Component can be rules or actions.
 */
function commerce_promotion_get_component_form($base_name, $form_state) {
  $component_name = $form_state['values'][$base_name . '_options'];
  $count = $form_state[$base_name . '_count'];

  $config = $form_state[$base_name . '_config'];
  $component_config = $config[$component_name];
  $component_config['component_name'] = $component_name;

  $form_callback = 'commerce_promotion_' . $base_name . '_item_form';
  return $form_callback($component_config, $count, $form_state);
}

function commerce_promotion_rules_item_form($config, $count, $form_state) {
  $form['operator'] = array(
    '#type' => 'select',
    '#options' => $config['operators'],
    '#field_prefix' => $config['name']
  );

  if ($config['field']) {
    // Quick and dirty!
    if ($config['component_name'] == 'zipcode_is') {
      $form['field'] = array(
        '#type' => 'textarea',
        '#cols' => 30,
      );
    } else if ($config['component_name'] == 'user_is') {
      $form['field'] = array(
        '#type' => 'textfield',
        '#autocomplete_path' => 'admin/commerce/promotions/user-autocomplete',
        '#multiple' => TRUE,
        '#size' => 40,
        '#description' => t('Enter comma separated user names.'),
      );
    } else {
      $form['field'] = array(
        '#type' => 'textfield',
        '#field_suffix' => 'USD',
        '#size' => 5,
      );
    }
  }

  $form['remove'] = array(
    '#type' => 'button',
    '#name' => 'rules-remove-' . $count,
    '#value' => t('Remove'),
    '#prefix' => ' &nbsp;&nbsp;&nbsp; ',
    '#ajax' => array(
      'callback' => 'commerce_promotion_ajax_rules_refresh',
      'wrapper' => 'promotion-rules',
    ),
  );

  return array($config['component_name'] => $form);
}

function commerce_promotion_actions_item_form($config, $count, $form_state) {
  if ($config['field']) {
    $form['field'] = array(
      '#type' => 'textfield',
      '#field_prefix' => $config['name'],
      '#field_suffix' => $config['component_name'] == 'discount_percentage' ? '%' : 'USD',
      '#size' => 5,
    );
  } else {

    $form['dummy'] = array(
      '#markup' => $config['name'],
    );
  }

  $form['remove'] = array(
    '#type' => 'button',
    '#name' => 'actions-remove-' . $count,
    '#value' => t('Remove'),
    '#prefix' => ' &nbsp;&nbsp;&nbsp; ',
    '#ajax' => array(
      'callback' => 'commerce_promotion_ajax_actions_refresh',
      'wrapper' => 'promotion-actions',
    ),
  );

  return array($config['component_name'] => $form);
}

/**
 * AJAX form callback: return updated rules conditions.
 */
function commerce_promotion_ajax_rules_refresh($form, $form_state) {
  return $form['rules']['rules_values'];
}

/**
 * AJAX form callback: return updated rules conditions.
 */
function commerce_promotion_ajax_actions_refresh($form, $form_state) {
  return $form['actions']['actions_values'];
}

/**
 * Page callback: autocomplete multiple user names.
 */
function commerce_promotion_user_autocomplete($string) {
  $matches = array();

  if ($string) {
    $items = array_map('trim', explode(',', $string));
    $last_item = array_pop($items);
    $prefix = !empty($items) ? implode(', ', $items) . ', ' : '';

    $result = db_select('users')->fields('users', array('name'))->condition('name', '%' . db_like($last_item) . '%', 'LIKE')->range(0, 10)->execute();
    foreach ($result as $user) {
      $matches[$prefix . $user->name] = $user->name;
    }
  }

  drupal_json_output($matches);
}

/**
 * Build the promotion code form.
 */
function commerce_promotion_set_codes_form(&$form, &$form_state, $promotion) {
  $form['activators']['code'] = array(
    '#type' => 'checkbox',
    '#title' => t('Has Promotion Code'),
    '#size' => 20,
  );

  $form['activators']['code_options'] = array(
    '#type' => 'fieldset',
    '#title' => t('Promotion Code'),
    '#states' => array(
      'visible' => array(
        'input[name="code"]' => array('checked' => TRUE),
      ),
    ),
  );

  $codes = &$form['activators']['code_options'];

  $codes['code_type'] = array(
    '#type' => 'radios',
    '#prefix' => '<div class="container-inline inline-radios">',
    '#suffix' => '</div><br />',
    '#options' => array('simple' => 'Code', 'bulk' => 'Bulk Code'),
    '#default_value' => $promotion['single_code'],
  );

  $codes['simple_code'] = array(
    '#type' => 'textfield',
    '#title' => t('Code'),
    '#size' => 20,
    '#states' => array(
      'visible' => array('input[name="code_type"]' => array('value' => 'simple'))
    ),
  );

  $bulk_states = array(
    'visible' => array('input[name="code_type"]' => array('value' => 'bulk'))
  );

  $codes['bulk_number'] = array(
    '#type' => 'textfield',
    '#title' => t('Number of Codes to generate'),
    '#size' => 5,
    '#states' => $bulk_states,
    '#description' => t('Bulk Promotion Codes cannot be edited. They can be regenerated as long as no code has been applied.'),
  );

  $codes['bulk_prefix'] = array(
    '#type' => 'textfield',
    '#title' => t('Code Prefix'),
    '#size' => 10,
    '#states' => $bulk_states,
    '#description' => t('For example: "ABC". Codes generated will follow the format "ABCXXXXX", where X is the auto-generated part of the code.'),
  );

  $codes['bulk_length'] = array(
    '#type' => 'select',
    '#title' => t('Code Length'),
    '#options' => range(8, 15),
    '#default_value' => $promotion['bulk_length'],
    '#description' => t('Includes the code prefix length.'),
    '#states' => $bulk_states,
  );

  $codes['usage_limit_per_code'] = array(
    '#type' => 'textfield',
    '#title' => t('Usage limit per code'),
    '#size' => 5,
    '#description' => t('How many times the code can be applied to an order. 0 for unlimited.'),
    '#default_value' => $promotion['usage_limit_per_code'],
  );

  $codes['usage_limit_per_user'] = array(
    '#type' => 'textfield',
    '#title' => t('Usage limit per user'),
    '#size' => 5,
    '#description' => t('How many times the code can be applied by the Order owner. 0 for unlimited.'),
    '#default_value' => $promotion['usage_limit_per_user'],
  );
}

/**
 * Build the date range form.
 */
function commerce_promotion_set_date_range_form(&$form, &$form_state, $promotion) {
  $form['activators']['date'] = array(
    '#type' => 'checkbox',
    '#title' => t('Is within date range'),
    '#size' => 20,
  );

  $states = array(
    'visible' => array(
      'input[name="date"]' => array('checked' => TRUE),
    ),
  );

  $form['activators']['date_range'] = array(
    '#type' => 'fieldset',
    '#title' => t('Date Range'),
    '#states' => $states,
  );

  $dates = &$form['activators']['date_range'];

  $dates['start'] = array(
    '#type' => 'date_popup',
    '#title' => t('Start Date'),
    '#states' => $states,
    '#date_year_range' => '-0:+10',
    '#date_format' => 'Y-m-d H:i',
    '#default_value' => date('Y-m-d H:i'),
  );

  $dates['end'] = array(
    '#type' => 'date_popup',
    '#title' => t('End Date'),
    '#states' => $states,
    '#date_year_range' => '-0:+10',
    '#date_format' => 'Y-m-d H:i',
    '#default_value' => date('Y-m-d H:i'),
  );
}
