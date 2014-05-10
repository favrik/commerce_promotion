<?php

/**
 * Implements hook_rules_data_info().
 */
function commerce_promotion_rules_data_info() {
  return array(
    'commerce_promotion' => array(
      'label' => t('commerce promotion'),
      'group' => t('Commerce Promotion'),
    ),
  );
}

/**
 * Implements hook_rules_event_info().
 */
function commerce_promotion_rules_event_info() {
  $items = array();

  $items['commerce_promotion_applied'] = array(
    'label' => t('Apply a promotion to a given order'),
    'group' => t('Commerce - promotion'),
    'variables' => array(
      'commerce_promotion' => array(
        'type' => 'commerce_promotion',
        'label' => t('Commerce promotion'),
      ),
    ),
  );

  return $items;
}


