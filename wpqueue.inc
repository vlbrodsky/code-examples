<?php
/**
 * @file
 * Definice tridy TWPQueue
 */

include_once('utils_file.inc');

// definice ukolu pqo queue
if (file_exists('/www/sys/queue_settings.inc')) include('/www/sys/queue_settings.inc');

/**
 * knihovna pro praci s frontou ukolu
 *
 * @version 0.1
 * @author Vladimir Brodsky
 */
class TWPQueue {

    /**
     * pocet sekund, ktere bude skript cekat v pripade, ze neni k dispozici zadny ukol
     */
    const SLEEPTIME_NO_TASK = 60;

    /**
     * typ projektu
     */
    protected $wp_typ;
    
    /**
     * kod fronty
     */
    protected $queueCode;
    
    /**
     * objekt pro praci s sql
     */
    protected $sql;

    /**
     * definice jednotlivych tasku. Definice je umistena v souboru /www/sys/queue_settings.inc
     */
    protected $taskDefinition = array();
    
    /**
     * konstruktor - nastavi defaultni hodnoty
     *
     * @return void
     */
    public function __construct($queueCode, $wp_typ = false)
    {
        $this->taskDefinition = $GLOBALS['sysQueueTaskDefinition'];
        if (!$wp_typ) {
            $wp_typ = $GLOBALS['wp_typ'];
        }
        $this->wp_typ = $wp_typ;
        $this->queueCode = $queueCode;
        $this->sql = new TWPSQL($GLOBALS['tr']);
    }

    /**
     * prida ukol do dane fronty
     * 
     * @param $task             identifikator ukolu
     * @param $params           pole parametru
     * @param $idUser           id uzivatele, jde o to, ze pokud vznikne chyba, 
     *                          spoustim skript znovu, ale cronem a do id uzivatele se ulozi predchozi
     *
     * @return void
     */
    public function addTask($task, $params, $idUser = false, $pocetSpus = 0)
    {
        if($this->getTaskDefinition($task)) {
            $params = array('wp_typ' => $this->wp_typ,
                            'kod_fronty' => $this->queueCode,
                            'ukol' => $task,
                            'parametry' => serialize($params),
                            'id_user' => $idUser ? $idUser : $GLOBALS['SYS_WEB']->privs()->getLoggedUserId(),
                            'pocetspusteni' => $pocetSpus,
                            );
            $this->sql->insert('fronta_ukol', $params);
        } else {
            throw new TWPQueueException ('QUEUE: neexistuje definice ukolu '.$task);
        }
    }
    
    /**
     * odstrani ukol s danym id z fronty ukolu
     * 
     * @param taskId    id ukolu
     * 
     * @return void
     */
    public function removeTask($taskId)
    {
        $this->sql->deleteRow('fronta_ukol', array('id' => $taskId));
    }

    /**
     * vybere nezpracovany ukol z fronty a spusti ho podle definice.
     * 
     * @return void
     */
    public function workOnQueue()
    {
        $task = reset($this->getPlannedTasks(1));
        if ($task) {
            $this->workOnTask($task);
        } else {
            // worker nema na cem pracovat
            $this->sleepNoTask();
        }
    }

    /**
     * zacne pracovat na danem ukolu, pokud existuje definice. Jinak zapise do logu chybu
     *
     * @param task      zaznam s ukolem
     * 
     * @return void
     */
    protected function workOnTask($task)
    {
        // zahajime cinnost na ukolu, aby zadny jiny worker nemohl zacit pracovat na stejnem ukolu
        $this->startWorkOnTask($task);

        // pokud neni nadefinovany ukol, tak nastavime, ze nastala chyba
        if (!$taskDefinition = $this->getTaskDefinition($task->UKOL)) {
            $this->errorTask($task, 'no task');
        } else {
            // jinak includneme pozadovany skript a spustime pozadovanou funkci
            $startTime = microtime(true);
            include($taskDefinition['script']);
            try {
                $result = call_user_func_array($taskDefinition['function'], array($task->WP_TYP, unserialize($task->PARAMETRY), $task->ID_USER, $task->POCETSPUSTENI));
                $this->finishWorkOnTask($task, $result, (microtime(true) - $startTime));
            } catch (Exception $e) {
                $this->errorTask($task, $e->getMessage());
            }
        }
    }

    /**
     * zapise do logu, ze neni na cem delat a pocka x sekund
     * 
     * @return void
     */
    protected function sleepNoTask()
    {
        $logData = array('queueCode'    => $this->queueCode,
                         'status'       => 'sleep',
                         'sleep'        => self::SLEEPTIME_NO_TASK,
                         );
        $this->log($logData);
        sleep(self::SLEEPTIME_NO_TASK);
    }
    
    /**
     * nastavi stav ukolu na prave se provadi
     * 
     * @param task      zaznam s ukolem
     *
     * @return void
     */
    protected function startWorkOnTask($task)
    {
        $this->sql->updateRow('fronta_ukol', array('stav' => 'P'), array('id' => $task->ID))->commit();
        $logData = array('queueCode'    => $this->queueCode,
                         'taskId'       => $task->ID,
                         'taskName'     => $task->UKOL,
                         'status'       => 'start',
                         );
        $this->log($logData);
    }

    /**
     * nastavi stav ukolu na ukonceno
     * 
     * @param task          zaznam s ukolem
     * @param $result       vysledek skriptu
     * @param $timeLength   delka provadeni samotneho ukolu
     *
     * @return void
     */
    protected function finishWorkOnTask($task, $result = '', $timeLength = 0)
    {
        $this->sql->updateRow('fronta_ukol',
                              array('stav' => 'V', 'vysledek' => $result, 'delkatrvani' => round($timeLength, 2)),
                              array('id' => $task->ID))->commit();
        $logData = array('queueCode'    => $this->queueCode,
                         'taskId'       => $task->ID,
                         'taskName'     => $task->UKOL,
                         'status'       => 'finish',
                         'message'      => $result,
                         'time'         =>  round($timeLength, 3),
                         );
        $this->log($logData);
    }

    /**
     * nastavi stav ukolu na chyba
     * 
     * @param task      zaznam s ukolem
     *
     * @return void
     */
    protected function errorTask($task, $result = '')
    {
        $this->sql->updateRow('fronta_ukol', array('stav' => 'E', 'vysledek' => $result), array('id' => $task->ID))->commit();
        $logData = array('queueCode'    => $this->queueCode,
                         'taskId'       => $task->ID,
                         'taskName'     => $task->UKOL,
                         'status'       => 'error',
                         'message'      => $result,
                         );
        $this->log($logData);
    }
    
    /**
     * vrati data daneho ukolu
     * 
     * @param taskId    id ukolu
     *
     * @return integer
     */
    protected function getTaskData($taskId)
    {
        return $this->sql->selectRow('fronta_ukol', array('id' => $taskId, 'kod_fronty' => $this->queueCode));
    }

    /**
     * vrati definici daneho ukolu
     * 
     * @param $task             identifikator ukolu
     *
     * @return array
     */
    protected function getTaskDefinition($task)
    {
        if(!$ret = $this->taskDefinition[$task]) {
            $ret = false;
        }
        return $ret;
    }

    /**
     * vrati celkovy pocet naplanovanych ukolu v dane fronte
     * 
     * @param $where        pole dodatecnych omezujicich podminek napr. wp_typ, id_user
     *
     * @return array
     */
    public function getCountOfPlannedTasks($where = array())
    {
        $sql = 'SELECT count(*) FROM fronta_ukol';
        $where = array_merge(array('kod_fronty' => $this->queueCode, 'stav' => 'C'), $where);
        $whereVals = array();
        foreach ($where as $whereKey => $whereValue) {
            $whereCols[] = $whereKey.'=?';
            $whereVals[] = $whereValue;
        }
        $sql .= ' WHERE '.implode(' AND ', $whereCols);
        return $this->sql->query($sql, $whereVals)->fetchValue();
    }

    /**
     * vrati seznam naplanovanych ukolu v dane fronte
     * 
     * @param $count        pocet vracenych ukolu
     * @param $where        pole dodatecnych omezujicich podminek napr. wp_typ, id_user
     *
     * @return array
     */
    public function getPlannedTasks($count = false, $where = array())
    {
        $whereArr = array_merge(array('kod_fronty' => $this->queueCode, 'stav' => 'C'), $where);
        return $this->loadTasks($count, $whereArr, 'priorita ASC');
    }
    
    /**
     * vrati seznam hotovych ukolu v dane fronte
     * 
     * @param $count        pocet vracenych ukolu
     * @param $where        pole dodatecnych omezujicich podminek napr. wp_typ, id_user
     *
     * @return void
     */
    public function getLastFinishedTasks($count = false, $where = array())
    {
        $whereArr = array_merge(array('kod_fronty' => $this->queueCode, 'stav' => 'V'), $where);
        return $this->loadTasks($count, $whereArr, 'datumukonceni DESC');
    }
    
    /**
     * vrati seznam aktualne provadenych ukolu
     * 
     * @param $where        pole dodatecnych omezujicich podminek napr. wp_typ, id_user
     *
     * @return array
     */
    public function getActualTasks($where = array())
    {
        $whereArr = array_merge(array('kod_fronty' => $this->queueCode, 'stav' => 'P'), $where);
        return $this->loadTasks(null, $whereArr, 'priorita ASC');
    }
    
    /**
     * vrati seznam ukolu, ktere skoncily s chybou
     * 
     * @param $count        pocet vracenych ukolu
     * @param $where        pole dodatecnych omezujicich podminek napr. wp_typ, id_user
     *
     * @return array
     */
    public function getErrorTasks($count = null,  $where = array())
    {
        $whereArr = array_merge(array('kod_fronty' => $this->queueCode, 'stav' => 'E'), $where);
        return $this->loadTasks($count, $whereArr, 'datumzahajeni DESC');
    }
    
    /**
     * vrati seznam pozadovanych ukolu
     * 
     * @param $count        pocet nactenych zaznamu
     * @param $where        pole s podminkami
     * @param $order        order
     *
     * @return array
     */
    protected function loadTasks($count, $where = array(), $order = null)
    {
        return $this->sql->select('fronta_ukol', $where, null, $count, $order)->fetchAll();
    }
    
    /**
     * zmeni prioritu ukolu na vyssi a tim posune ukol ve fronte dopredu. Osetreni se provadi v triggeru
     * 
     * @param taskId    id ukolu
     *
     * @return void
     */
    public function changePriorityUp($taskId)
    {
        $this->changePriority($taskId, -1);
    }

    /**
     * zmeni prioritu ukolu na nizsi a tim posune ukol ve fronte dozadu. Osetreni se provadi v triggeru
     * 
     * @param taskId    id ukolu
     *
     * @return void
     */
    public function changePriorityDown($taskId)
    {
        $this->changePriority($taskId, +1);
    }

    /**
     * prohodi prioritu dvou ukolu mezi sebou
     * 
     * @param taskId    id ukolu
     * @param diff      pocet pozic, o kolik pozunuju
     *
     * @return void
     */
    protected function changePriority($taskId, $diff)
    {
        $taskData = $this->getTaskData($taskId);

        if ($taskData->ID) {
            $priorityOriginal = $taskData->PRIORITA;
            $priorityNew = $priorityOriginal + $diff;
    
            // aktualizujeme prioritu u prohazovaneho ukolu
            $whereArr = array('kod_fronty' => $this->queueCode, 'priorita' => $priorityNew);
            $this->sql->updateRow('fronta_ukol', array('priorita' => $priorityOriginal), $whereArr);
    
            // aktualizujeme prioritu u daneho ukolu
            $whereArr = array('id' => $taskId);
            $this->sql->updateRow('fronta_ukol', array('priorita' => $priorityNew), $whereArr);
        }
    }

    /**
     * ulozi radek do log souboru
     * 
     * @param $data     data, ktera chceme logovat
     *
     * @return void
     */
    protected function log($logData)
    {
        TWPjsonLog::log(CACHE_LOGDIR.'/queue.log', $logData);
    }




}

