<?php
/**
 * @file
 * ApacheSolrQueue functionality.
 */

/**
 * Default queue implementation.
 */
class ApacheSolrQueue implements DrupalReliableQueueInterface {
  protected $queueTable = 'apachesolr_index_queue';

  /**
   * Get apachesolr index queue table name.
   *
   * @return string
   *   Queue table name
   */
  public function getQueueTable() {
    return $this->queueTable;
  }
  /**
   * Add a queue item and store it directly to the queue.
   *
   * @param ApacheSolrQueueItem $item
   *   Data to be associated with the new task in the queue.
   * 
   * @return bool
   *   TRUE if the item was successfully created and was (best effort) added
   *   to the queue, otherwise FALSE. We don't guarantee the item was
   *   committed to disk etc, but as far as we know, the item is now in the
   *   queue.
   */
  public function createItem($item) {
    $query = db_insert($this->queueTable)
      ->fields(array(
        'entity_type' => $item->getEntityType(),
        'entity_id' => $item->getEntityId(),
        'bundle' => $item->getBundle(),
        'status' => $item->getStatus(),
        // We cannot rely on REQUEST_TIME because many items might be created
        // by a single request which takes longer than 1 second.
        'created' => time(),
      ));
    return (bool) $query->execute();
  }

  /**
   * Retrieve the number of items in the queue.
   *
   * @inheritdoc
   */
  public function numberOfItems() {
    $count = db_select($this->queueTable)->fields(NULL, array('item_id'))
      ->countQuery()->execute()->fetchField();
    return $count;
  }

  /**
   * Retrieve roughly the number of items in the queue.
   *
   * @return int
   *   Number of items remaining for entity type to process in queue.
   */
  public function numberOfItemsEstimate() {
    // Use rough table_row estimates for performance.
    return db_query('SELECT TABLE_ROWS FROM {INFORMATION_SCHEMA.TABLES} WHERE TABLE_NAME = :table_name', array(':table_name' => $this->queueTable))->fetchField();
  }

  /**
   * Claim an item in the queue for processing.
   *
   * @inheritdoc
   */
  public function claimItem($lease_time = 30) {
    $items = $this->claimItemMultiple(1, $lease_time, '');
    $item = array_shift($items);
    return $item;
  }

  /**
   * Claim multiple items in the queue for processing.
   *
   * @param int $num_items
   *   Number of items to retrieve.
   * @param int $lease_time
   *   Time in seconds to keep lock on items.
   * @param string $entity_type
   *   Entity type to filter items on.
   *
   * @return array
   *   Array of queue row results as objects, or FALSE if none are available.
   */
  public function claimItemMultiple($num_items, $lease_time = 30, $entity_type = '') {
    if (!$num_items) {
      return FALSE;
    }
    // Claim an item by updating its expire fields. If claim is not successful
    // another thread may have claimed the item in the meantime. Therefore loop
    // until an item is successfully claimed or we are reasonably sure there
    // are no unclaimed items left.
    while (TRUE) {
      $query = db_select($this->queueTable, 'q');
      $query->fields('q')
        ->condition('q.expire', 0)
        ->range(0, $num_items);
      if (!empty($entity_type)) {
        $query->condition('q.entity_type', $entity_type);
      }

      $queue_items = $query->execute()->fetchAllAssoc('item_id');
      if ($queue_items) {
        // Try to update the item. Only one thread can succeed in UPDATEing the
        // same row. We cannot rely on REQUEST_TIME because items might be
        // claimed by a single consumer which runs longer than 1 second. If we
        // continue to use REQUEST_TIME instead of the current time(), we steal
        // time from the lease, and will tend to reset items before the lease
        // should really expire.

        /** @var UpdateQuery $update */
        $update = db_update($this->queueTable)
          ->fields(array(
            'expire' => time() + $lease_time,
          ))
          ->condition('item_id', array_keys($queue_items), 'IN')
          ->condition('expire', 0);
        // If there are affected rows, this update succeeded.
        if ($update->execute()) {
          return $queue_items;
        }
      }
      else {
        // No items currently available to claim.
        return FALSE;
      }
    }
    return FALSE;
  }

  /**
   * Requeue expired queue items for processing.
   */
  public function requeueExpired() {
    db_update($this->queueTable)
      ->fields(array(
        'expire' => 0,
      ))
      ->condition('expire', 0, '>')
      ->condition('expire', "now()", '<');
  }

  /**
   * Release item from queue.
   *
   * @param ApacheSolrQueueItem $item
   *   Item to release with item_id set.
   *
   * @return bool
   *   True if release was successful.
   */
  public function releaseItem($item) {
    return $this->releaseItemMultiple(array($item));
  }

  /**
   * Release multiple items from queue.
   *
   * @param array $item_ids
   *   Array of item_ids to release.
   *
   * @return bool
   *   True if release was successful.
   */
  public function releaseItemMultiple($item_ids) {
    /** @var UpdateQuery $update */
    $update = db_update($this->queueTable)
      ->fields(array(
        'expire' => 0,
      ))
      ->condition('item_id', $item_ids, 'IN');
    return $update->execute();
  }

  /**
   * Delete item from queue.
   *
   * @param ApacheSolrQueueItem $item
   *   Item to delete with item_id set.
   */
  public function deleteItem($item) {
    $this->deleteItemMultiple(array($item));
  }

  /**
   * Delete multiple items from queue.
   *
   * @param array $item_ids
   *   Array of item_ids to delete.
   */
  public function deleteItemMultiple($item_ids) {
    db_delete($this->queueTable)
      ->condition('item_id', $item_ids, 'IN')
      ->execute();
  }

  /**
   * Delete item from queue by Entity Type and ID.
   *
   * @param string $entity_type
   *   Entity Type of item to delete
   * @param int $entity_id
   *   Entity Id of item to delete
   */
  public function deleteItemByEntityTypeId($entity_type, $entity_id) {
    $this->deleteItemMultipleByEntityTypeId($entity_type, array($entity_id));
  }

  /**
   * Delete multiple items from queue by Entity Type and ID.
   *
   * @param string $entity_type
   *   Entity type of items to delete.
   * @param array $entity_ids
   *   Array of entity_ids.
   */
  public function deleteItemMultipleByEntityTypeId($entity_type, $entity_ids) {
    db_delete($this->queueTable)
      ->condition('entity_id', $entity_ids, 'IN')
      ->condition('entity_type', $entity_type)
      ->execute();
  }

  /**
   * Delete multiple items from queue by bundle with status = 1.
   *
   * @param string $bundle
   *   Bundle of items to delete.
   */
  public function deleteItemMultipleByBundle($bundle) {
    db_delete($this->queueTable)
      ->condition('bundle', $bundle)
      ->condition('status', 1)
      ->execute();
  }

  /**
   * Delete multiple items from queue by entity_type with status = 1.
   *
   * @param string $entity_type
   *   Entity type of items to delete.
   */
  public function deleteItemMultipleByEntityType($entity_type) {
    db_delete($this->queueTable)
      ->condition('entity_type', $entity_type)
      ->condition('status', 1)
      ->execute();
  }
  /**
   * Delete multiple items from queue by status.
   *
   * @param int $status
   *   Status of items to delete.
   */
  public function deleteItemMultipleByStatus($status) {
    db_delete($this->queueTable)
      ->condition('status', $status)
      ->execute();
  }

  /**
   * Create a queue.
   *
   * @inheritdoc
   */
  public function createQueue() {
    // All tasks are stored in a single database table (which is created when
    // Apachesolr is first installed) so there is nothing to do to create
    // a new queue.
  }

  /**
   * Delete a queue and every item in the queue.
   */
  public function deleteQueue() {
    db_delete($this->queueTable)
      ->execute();
  }
}

class ApacheSolrQueueItem {
  protected $itemId;
  protected $entityType;
  protected $entityId;
  protected $bundle;
  protected $status;
  protected $expire;
  protected $created;

  /**
   * Get accessor for $this->itemId.
   * @return int
   *   Item id of item.
   */
  public function getItemId() {
    return $this->itemId;
  }

  /**
   * Set accessor for $this->itemId.
   *
   * @param int $item_id
   *   Item ID of item.
   */
  public function setItemId($item_id) {
    $this->itemId = $item_id;
  }

  /**
   * Get accessor for $this->entityType.
   * @return string
   *   Entity type of item.
   */
  public function getEntityType() {
    return $this->entityType;
  }

  /**
   * Set accessor for $this->entityType.
   *
   * @param string $entity_type
   *   Entity type of item.
   */
  public function setEntityType($entity_type) {
    $this->entityType = $entity_type;
  }

  /**
   * Get accessor for $this->entityId.
   * @return int
   *   Entity id of item.
   */
  public function getEntityId() {
    return $this->entityId;
  }

  /**
   * Set accessor for $this->entityId.
   *
   * @param int $entity_id
   *   Entity ID of item.
   */
  public function setEntityId($entity_id) {
    $this->entityId = $entity_id;
  }

  /**
   * Get accessor for $this->bundle.
   * @return string
   *   Bundle of item.
   */
  public function getBundle() {
    return $this->bundle;
  }

  /**
   * Set accessor for $this->bundle.
   *
   * @param string $bundle
   *   Bundle of item.
   */
  public function setBundle($bundle) {
    $this->bundle = $bundle;
  }

  /**
   * Get accessor for $this->status.
   * @return int
   *   Status of item. 1 is for index addition, 0 is for index removal.
   */
  public function getStatus() {
    return $this->status;
  }

  /**
   * Set accessor for $this->status.
   *
   * @param int $status
   *   Status of item. 1 is for index addition, 0 is for index removal.
   */
  public function setStatus($status) {
    $this->status = $status;
  }

  /**
   * Get accessor for $this->expire.
   * @return int
   *   Item's unix timestamp of expire.
   */
  public function getExpire() {
    return $this->expire;
  }

  /**
   * Set accessor for $this->expire.
   *
   * @param int $expire
   *   Epoch (UNIX style) time stamp
   */
  public function setExpire($expire) {
    $this->expire = $expire;
  }

  /**
   * Get accessor for $this->Created.
   * @return int
   *   Item's unix timestamp of created.
   */
  public function getCreated() {
    return $this->created;
  }

  /**
   * Set accessor for $this->created.
   *
   * @param int $created
   *   Epoch (UNIX style) time stamp.
   */
  public function setCreated($created) {
    $this->created = $created;
  }

  /**
   * Convert ApacheSolrQueueItem to an object.
   *
   * @return object
   *   StdClass object of queue item properties.
   */
  public function toObject() {
    $obj = new StdClass();
    $obj->item_id = $this->getItemId();
    $obj->entity_type = $this->getEntityType();
    $obj->entity_id = $this->getEntityId();
    $obj->bundle = $this->getBundle();
    $obj->status = $this->getStatus();
    $obj->expire = $this->getExpire();
    $obj->created = $this->getCreated();
    $obj->changed = $this->created;
    return $obj;
  }
}
