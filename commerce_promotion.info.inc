<?php

class CommercePromotionMetadataController extends EntityDefaultMetadataController {

  public function entityPropertyInfo() {
    $info = parent::entityPropertyInfo();
    $properties = &$info[$this->type]['properties'];

    $properties['status']['type'] = 'boolean';

    return $info;
  }

}
