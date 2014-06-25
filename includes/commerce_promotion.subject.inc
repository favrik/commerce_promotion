<?php

use Balalaika\Entity\PromotionSubjectInterface;

class CommercePromotionSubject implements PromotionSubjectInterface {
  protected $order;
  protected $order_wrapper;
  protected $promotion_data;
  protected $line_item_types = array('product');

  /**
   * Constructor.
   *
   * @param object $order
   * @param array $promotion_data
   */
  public function __construct($order, array $promotion_data = array()) {
    $this->order = $order;
    $this->order_wrapper = entity_metadata_wrapper('commerce_order', $order);
    $this->promotion_data = $promotion_data;
  }

  /**
   * Get order total.
   *
   * @return int
   */
  public function getOrderTotal() {
    return $this->order_wrapper->commerce_order_total->amount->value();
  }

  /**
   * Get product line item total.
   *
   * @return int
   */
  public function getProductTotal() {
    $total = 0;

    if (!$this->order_wrapper->commerce_line_items->value()) {
      return $total;
    }

    foreach ($this->order_wrapper->commerce_line_items as $line_item_wrapper) {
      if (in_array($line_item_wrapper->type->value(), $this->line_item_types)) {
        $line_item_total = commerce_price_wrapper_value($line_item_wrapper, 'commerce_total', TRUE);
        $total += $line_item_total['amount'];
      }
    }

    return $total;
  }

  /**
   * Add a discount to the order total.
   *
   * @param int $discount
   */
  public function addOrderDiscount($discount) {
    $commerce_price = array(
      'amount' => -$discount, // Notice the minus sign here.
      'currency_code' => commerce_default_currency(),
      'data' => array(),
    );

    if (!$this->setExistingPromotionLineItem($commerce_price)) {
      $this->addPromotionLineItem($commerce_price);
    }

    commerce_order_calculate_total($this->order);
  }

  /**
   * Usually called on the commerce_promotion checkout pane, NOT within the cart_order_refresh flow.
   * That's why here we save the order.
   */
  public function savePromotionReference() {
    // Note: $delta is always 0.
    $this->order->field_commerce_promotions[LANGUAGE_NONE][0]['target_id'] = $this->promotion_data['commerce_promotion_id'];
    $this->order_wrapper->field_commerce_promotion_code->set($this->promotion_data['submitted_code']);
    commerce_order_save($this->order);
  }

  public function setPromotionReference($order) {
    $order->field_commerce_promotions[LANGUAGE_NONE][0]['target_id'] = $this->promotion_data['commerce_promotion_id'];
    $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
    $order_wrapper->field_commerce_promotion_code->set($this->promotion_data['submitted_code']);
  }

  protected function setExistingPromotionLineItem($amount) {
    $modified_existing = FALSE;
    foreach ($this->order_wrapper->commerce_line_items as $line_item_wrapper) {
      if ($line_item_wrapper->getBundle() == 'commerce_promotion') {
        // Add the discount component price if the line item was originally
        // added by discount module.
        $line_item = $line_item_wrapper->value();
        if (isset($line_item->data['commerce_promotion'])) {
          $this->resetPromotionLinePrice($line_item_wrapper, $amount);
          $this->addPriceComponent($line_item_wrapper, $amount);
          $line_item_wrapper->save();
          $modified_existing = TRUE;
        }
      }
    }

    return $modified_existing;
  }

  protected function addPromotionLineItem($amount) {
    // Create a new line item.
    $values = array(
      'type' => 'commerce_promotion',
      'order_id' => $this->order_wrapper->order_id->value(),
      'quantity' => 1,
      // Flag the line item.
      'data' => array('commerce_promotion' => $this->promotion_data),
    );

    $line_item = entity_create('commerce_line_item', $values);
    $line_item_wrapper = entity_metadata_wrapper('commerce_line_item', $line_item);

    $this->resetPromotionLinePrice($line_item_wrapper, $amount);

    // Add the discount price component.
    $this->addPriceComponent($line_item_wrapper, $amount);

    // Save the line item and add it to the order.
    $line_item_wrapper->save();
    $this->order_wrapper->commerce_line_items[] = $line_item_wrapper;
  }

  public function resetPromotionLinePrice($line_item_wrapper, $amount) {
    // Initialize the line item unit price.
    $line_item_wrapper->commerce_unit_price->amount = 0;
    $line_item_wrapper->commerce_unit_price->currency_code = $amount['currency_code'];

    // Reset the data array of the line item total field to only include a
    // base price component, set the currency code from the order.
    $base_price = array(
      'amount' => 0,
      'currency_code' => $amount['currency_code'],
      'data' => array(),
    );
    $line_item_wrapper->commerce_unit_price->data = commerce_price_component_add(
      $base_price, 'base_price', $base_price, TRUE);
  }

  public function addPriceComponent(EntityDrupalWrapper $line_item_wrapper, $amount) {
    $unit_price = commerce_price_wrapper_value($line_item_wrapper, 'commerce_unit_price', TRUE);
    $current_amount = $unit_price['amount'];
    // Currencies don't match, abort.
    if ($amount['currency_code'] != $unit_price['currency_code']) {
      return;
    }

    // Calculate the updated amount and create a price array representing the
    // difference between it and the current amount.
    $updated_amount = commerce_round(COMMERCE_ROUND_HALF_UP, $current_amount + $amount['amount']);

    $difference = array(
      'amount' => $amount['amount'],
      'currency_code' => $amount['currency_code'],
      'data' => array(
        'promotion_id' => $this->promotion_data['commerce_promotion_id'],
        'promotion_name' => $this->promotion_data['name'],
        'submitted_code' => $this->promotion_data['submitted_code'],
      ),
    );

    // Set the new unit price.
    $line_item_wrapper->commerce_unit_price->amount = $updated_amount;
    // Add the discount amount as a price component.
    $price = $line_item_wrapper->commerce_unit_price->value();
    $line_item_wrapper->commerce_unit_price->data = commerce_price_component_add(
      $price, 'promotion', $difference, TRUE, TRUE);

    // Update commerce_total so that the order total can be refreshed without
    // saving the line item. Taken from CommerceLineItemEntityController::save().
    $quantity = $line_item_wrapper->quantity->value();

    // Update the total of the line item based on the quantity and unit price.
    $unit_price = commerce_price_wrapper_value($line_item_wrapper, 'commerce_unit_price', TRUE);

    $line_item_wrapper->commerce_total->amount = $quantity * $unit_price['amount'];
    $line_item_wrapper->commerce_total->currency_code = $unit_price['currency_code'];

    // Add the components multiplied by the quantity to the data array.
    if (empty($unit_price['data']['components'])) {
      $unit_price['data']['components'] = array();
    }
    else {
      foreach ($unit_price['data']['components'] as $key => &$component) {
        $component['price']['amount'] *= $quantity;
      }
    }

    // Set the updated data array to the total price.
    $line_item_wrapper->commerce_total->data = $unit_price['data'];
  }

  /**
   * Sets the order shipping to 0 added as a discount.  TODO: this could be more abstract
   * and only use one method: "AddPromotion()"
   *
   * Enforced by rule event from commerce_shipping.  Here we just add the discount to the order.
   */
  public function setFreeShipping() {
    // This is really a workaround because the commerce_cart_order_refresh event does not 
    // save the order unless the original and modified order are different, or the line items
    // have changed.  When a promotion only has the Free shipping action enabled, there are no
    // line items changed.
    $this->order->has_free_shipping = TRUE;
  }

  /**
   * @return boolean
   */
  public function isFirstTimeCustomer() {
    // Anonymous users are not first customers.
    if (!$this->order->uid) {
      return FALSE;
    }

    $query = $this->getBaseOrderQuery();
    $query->propertyCondition('uid', $this->order->uid, '=');
    $count = $query->count()->execute();
    if ($count) {
      return FALSE;
    }

    return TRUE;
  }

  /**
   * Get the shipping address zipcode.
   *
   * @return int
   */
  public function getShippingZipcode() {
    $customer_shipping = $this->order_wrapper->commerce_customer_shipping;
    if (isset($customer_shipping->commerce_customer_address)) {
      return $customer_shipping->commerce_customer_address->postal_code->value();
    }

    return 0;
  }

  /**
   * Get the user entity identifier.
   *
   * @return int
   */
  public function getUserId() {
    return $this->order->uid;
  }

  /**
   * Get a DateTime value according to the system settings.
   *
   * @param mixed $dateTime - Commonly a string (2014-04-16 06:00) or an int for timestamps.
   */
  public function getDateTime($dateTime = null) {
    return new DateObject($dateTime);
  }

  /**
   * Get a comparable DateTime value using the system settings.
   *
   * @return mixed
   */
  public function getCurrentDateTime() {
    return new DateObject();
  }

  /**
   * Get the customer submitted promotion code. 
   *
   * @return string
   */
  public function getPromotionCode() {
    return $this->promotion_data['submitted_code'];
  }

  /**
   * Get how many times the submitted promotion code has been used by the system.
   *
   * This needs the code_id in commerce_promotion_codes table.
   *
   * @return int
   */
  public function getPromotionUsageCount() {
    $query = $this->getBaseOrderQuery();
    $query->fieldCondition('field_commerce_promotions', 'target_id', $this->promotion_data['commerce_promotion_id']);
    $query->fieldCondition('field_commerce_promotion_code', 'value', $this->promotion_data['submitted_code']);
    $count = $query->count()->execute();

    return $count;
  }

  /**
   * Get how many times this promotion has been used by the order owner.
   *
   * @return int
   */
  public function getPromotionUserCount() {
    $query = $this->getBaseOrderQuery();
    $query
      ->fieldCondition('field_commerce_promotions', 'target_id', $this->promotion_data['commerce_promotion_id'])
      ->fieldCondition('field_commerce_promotion_code', 'value', $this->promotion_data['submitted_code'])
      ->propertyCondition('uid', $this->order->uid);

    return $query->count()->execute();
  }

  /**
   * Get basic order entity query with default order placed statuses.
   *
   * @return object EntityFieldQuery
   */
  public function getBaseOrderQuery() {
    $query = new EntityFieldQuery();
    $query
      ->entityCondition('entity_type', 'commerce_order', '=')
      ->propertyCondition('status', $this->placedOrderStatuses(), 'IN');

    return $query;
  }

  /**
   * Get list of order statuses where the customer has already paid.
   *
   * @return array
   */
  private function placedOrderStatuses() {
    return array('pending', 'processing', 'completed', 'cardonfile_charged');
  }
}
