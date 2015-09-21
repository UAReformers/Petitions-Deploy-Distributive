<?php
/**
 * @file
 * Define a MigrateSource for importing from MongoDB connections
 */

/**
 * Implementation of MigrateSource, to handle imports from MongoDB connections.
 */
class MigrateSourceMongoDB extends MigrateSource {
  /**
   * The mongodb collection object.
   *
   * @var MongoCollection
   */
  protected $collection;

  /**
   * The mongodb cursor object.
   *
   * @var MongoCursor
   */
  protected $cursor;

  /**
   * The mongodb query.
   *
   * @var array
   */
  protected $query;

  /**
   * List of available source fields.
   *
   * @var array
   */
  protected $fields = array();

  /**
   * Simple initialization.
   */
  public function __construct(MongoCollection $collection, array $query, array $fields = array(), array $sort = array('_id' => 1), array $options = array()) {
    parent::__construct($options);

    $this->collection = $collection;
    $this->query = $query;
    $this->sort = $sort;
    $this->fields = $fields;

    // get all indexes from collection
    $indexes = $this->collection->getIndexInfo();

    // check if index for drupalMigration data exist
    $create_index = TRUE;
    foreach ($indexes as $index) {
      if ($index['name'] == '_drupalMigration_') {
        $create_index = FALSE;
        break;
      }
    }

    // ToDo: delete index if is not neccessary (0 migrated items?)
  }

  /**
   * Returns a list of fields available to be mapped from the source query.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields() {
    // The fields are passed to the constructor for this plugin.
    return $this->fields;
  }

  /**
   * Return a count of all available source records.
   */
  public function computeCount() {
    return $this->cursor->count(TRUE);
  }

  /**
   * Implementation of MigrateSource::getNextRow().
   *
   * @return object
   */
  public function getNextRow() {
    $row = $this->cursor->getNext();

    if ($row) {
      return (object) $row;
    }

    return NULL;
  }

  /**
   * Implementation of MigrateSource::performRewind().
   *
   * @return void
   */
  public function performRewind() {
    // If we have existing idlist we use them.
    if ($this->idList) {
      foreach ($this->idList as $key => $id) {
        $this->idList[$key] = $this->getMongoId($id);
      }

      $this->query['_id']['$in'] = $this->idList;
    }
    else {
      // ToDo: find some method for better performance because for now
      // we get always all (migrated and unmigrated) rows
      // Now we use method witch require complete and completeRollbacks method
      // in migration class.
      $this->query['drupalMigration'] = NULL;
    }

    migrate_instrument_start('MigrateSourceMongoDB execute');
    $this->cursor = $this->collection->find($this->query);
    $this->cursor->sort($this->sort);
    migrate_instrument_stop('MigrateSourceMongoDB execute');
  }

  /**
   * Return a string representing the source query.
   *
   * @return string
   */
  public function __toString() {
    if (is_null($this->cursor)) {
      $this->cursor = $this->collection->find($this->query);
      $this->cursor->sort($this->sort);
    }

    return (string) drupal_json_encode($this->cursor->info());
  }

  /**
   * Migration document complete - set migration info into migrated document.
   *
   * @param type $document_id
   * @param type $destination_id
   * @param type $timestamp
   */
  public function setMongoDocumentMigrationInfo($document_id, $destination_id, $timestamp) {
    $result = $this->collection->update(
      array('_id' => $this->getMongoId($document_id)),
      array(
        '$set' => array(
          'drupalMigration.destinationID' => $destination_id,
          'drupalMigration.timestamp' => $timestamp,
        ),
      )
    );
  }

  /**
   * Rollback complete - remove migration info from document.
   *
   * @param type $destination_id
   */
  public function removeMongoDocumentMigrationInfo($destination_id) {
    if (!is_array($destination_id)) {
      $destination_id = array($destination_id);
    }

    $result = $this->collection->update(
      array('drupalMigration.destinationID' => array('$in' => $destination_id)),
      array(
        '$unset' => array(
          'drupalMigration' => 1,
        ),
      ),
      array(
        'multiple' => 1,
      )
    );
  }

  /**
   * Check if given id is mongo ObjectId and return id as mongo ObjectId.
   *
   * @param type $id
   * @return type
   */
  private function getMongoId($id) {
    // Trying create Mongo ObjectId
    $mongoid = new MongoId($id);
    // If (string)$mongoid != $document_id we simple use $document_id as $mongoid
    if ((string) $mongoid != $id) {
      $mongoid = $id;
    }

    return $id;
  }
}
