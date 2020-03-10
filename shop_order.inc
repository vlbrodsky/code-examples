<?php

    include_once(GLOBAL_PATH.'/library/shop_order_item.inc');
    include_once(GLOBAL_PATH.'/library/customer.inc');


    // trida pro objednavku, pomoci ni se generuje kosik, pokladna atd
    class shopOrder
    {
        // definice povolenych stavu objednavky
        const STATUSES_ORDER = array('pending' => 'o-pending', 'processing' => 'o-processing', 'shipped' => 'o-shipped', 'completed' => 'o-completed', 'cancelled' => 'o-cancelled');
        
        // definice povolenych stavu poptavky
        const STATUSES_DEMAND = array('pending' => 'd-pending', 'processing' => 'd-processing', 'cancelled' => 'd-cancelled');

        // definice vsech stavu tj. objednavky i poptavky, ktere jsou uzavrene
        const STATUSES_COMPLETED = array('o-completed', 'o-cancelled', 'd-cancelled');

        // definice defaultniho stavu
        const STATUS_CONCEPT = 'concept';

        // zakladni objekt pro system
        private $system;

        // zakladni objekt pro shop
        public $shop;

        // priznak jestli se jedna o historii objednavky
        private $isHistory;
        
        // id objednavky v db
        private $id;
        
        // data vyctena z db
        private $dbData = array();

        // kod objednavky
        private $code;

        // id bioplynky na objednavce
        private $customerId;
        
        // datum vytvoreni objednavky
        private $dateOrigin;

        // datum zmeny objednavky
        private $dateChange;

        // celkova cena objednavky
        private $price = false;

        // produkty na objednavce
        private $items = array();

        // stav objednavky
        private $status;
        
        // aktualne prihlaseny zakaznik tj. bioplynka
        private $customer; 
                        
        // sleva
        private $discount = false;
                
        // slevovy kod
        private $discountCode = false;
                
        // fakturacni adresa
        private $addressBilling = false;

        // dodaci adresa
        private $addressDelivery = false;
                    
        // kontaktni osoba
        private $contactPerson = false;

        // dodaci adresa je stejne jako fakturacni
        private $addressIsSame = 0;
                    
        // odeslat vse dohromady
        private $sendTogether = 0;
                    
        // aktualizovat dodaci adresu v adresari
        private $addressUpdate = 0;
                    
        // poznamka
        private $comment = false;

        // poznamka eshou
        private $commentEshop = false;

        // url pro sledovani zasilky
        private $shippingUrl = false;
                    
        // texty odeslane maily v poli
        private $emailTexts = false;
                    
        // konstruktor - pripravi zakladni data
        public function __construct($system, $shop, $isHistory = false)
        {
            $this->system = $system;
            $this->shop = $shop;
            $this->status = $this::STATUS_CONCEPT;
            $this->isHistory = $isHistory;
        }

        
        // definice vsech stavu tj. objednavky i poptavky, ktere nejsou uzavrene
        static public function getStatusesOpened()
        {
            return array_diff(array_merge(array_values(shopOrder::STATUSES_ORDER), array_values(shopOrder::STATUSES_DEMAND)), shopOrder::STATUSES_COMPLETED);
        }
        

        // nacte objednavku ze session
        public function loadFromSession()
        {
            $this->setDataFromArr($_SESSION['shop']['order']);
        }
        
        
        // ulozi objednavku do session
        public function saveToSession()
        {
            $_SESSION['shop']['order'] = $this->prepareDataForSave();
        }

    
        // vycistime celou objednavku ze session
        public function clearSession()
        {
            unset($_SESSION['shop']['order']);
        }
        
        
        // nacte objednavku z databaze podle id
        public function loadFromDb($id)
        {
            $dataArr = $this->loadDataFromDb(array('id = '.(integer)$id));
            if (!empty($dataArr)) {
                $this->setFromDbRow(reset($dataArr));
            }
        }


        // nacte objednavku z databaze - pouze data
        public function loadDataFromDb($sqlWhereArr, $sqlOrder = false)
        {   
            $ordersArr = array();
            
            if (!is_array($sqlWhereArr)) {
                $sqlWhereArr = array();
            }

            if ($sqlOrder) {
                $sqlOrder = ' ORDER BY '.$sqlOrder;
            } else {
                $sqlOrder = '';
            }
            
            if ($this->isHistory) {
                $table = 'eshop_order_history';
            } else {
                $table = 'eshop_order';
            }

            $sql = "SELECT id, code, id_user_change, date_origin, date_change, id_biogas, price, status, data_actual,
                            (SELECT jmeno FROM uzivatel u WHERE u.id=o.id_user_origin) AS id_user_origin_name,
                            (SELECT jmeno FROM uzivatel u WHERE u.id=o.id_user_change) AS id_user_change_name,
                            (SELECT nazev FROM bioplynka b WHERE b.id=o.id_biogas) AS id_biogas_name
                        FROM ".$table." o
                        WHERE ".join(' AND ', $sqlWhereArr).$sqlOrder;
//                         _D($sql, 1);
            $res = $this->system->db->query($sql);
            while ($row = $res->fetch_assoc()) {
                $ordersArr[$row['id']] = $row;
            }
            
            return $ordersArr;
        }


        // nastavi data objednavky podle dat vyctenych z db
        public function setFromDbRow($dataRow)
        {
            $this->setId($dataRow['id']);
            $this->setCode($dataRow['code']);
            $this->setDateOrigin(strtotime($dataRow['date_origin']));
            $this->setDateChange(strtotime($dataRow['date_change']));
            $this->setCustomerId($dataRow['id_biogas']);
            $this->setPrice($dataRow['price']);
            $this->setStatus($dataRow['status']);
            $this->setDataFromArr(json_decode($dataRow['data_actual'], true));
            $this->setDbData($dataRow);
        }


        // ulozi objednavku do databaze
        public function saveToDb()
        {
            $data = array();
            $data['code'] = $this->getCode();
            $data['id_user_change'] = $this->system->priv->getUserId();
            $data['id_biogas'] = $this->getCustomerId();
            $data['price'] = $this->getPrice();
            $data['status'] = $this->getStatus();
            // pokud objednavka neobsahuje dopravne, tak ho pridame
            if (!$this->shippingIncluded()) {
                $this->addItem(SHOP_SHIPPING);
            } else {
                // seradime produkty tak, aby posledni bylo dopravne kvuli pripadu pridani novych produktu
                $this->moveShippingEnd();
            }
            // ziskame data pro ulozeni do jsonu
            $data['data_actual'] = json_encode($this->prepareDataForSave());
            // ulozime do db
            $orderId = saveDataToDb($this->system->db, 'eshop_order', $this->getId(), $data);
            // ulozime itemy do db
            foreach ($this->getAllItems() AS $productCode => $item) {
                $item->saveToDb($orderId);
            }
        }
        

        // presune polozku s dopravnym na konec polozek
        private function moveShippingEnd()
        {
            $shippingItem = false;
            $shippingIndex = false;
            foreach ($this->getAllItems() AS $key => $item) {
                if ($item->isShipping()) {
                    $shippingItem = $item;
                    $shippingIndex = $key;
                }
            }
            // pokud jsme dohledali dopravne, tak ho odstranime a pridame znovu na konec
            if (!empty($shippingItem)) {
                unset($this->items[$shippingIndex]);
                $this->items[$shippingIndex] = $shippingItem;
            }
        }
        
        
        // vrati true pokud se jedna o poptavku tj. minimalne jeden produkt ma nulovou cenu
        public function isDemand()
        {
            $ret = false;
            foreach ($this->getAllItems() AS $item) {
                if ($item->isDemand()) {
                    $ret = true;
                    break;
                }
            }
            return $ret;
        }
        
        
        // vrati true pokud se jedna o poptavku podle stavu
        public function isDemandStatus()
        {
            if (in_array($this->getStatus(), $this::STATUSES_DEMAND)) {
                $ret = true;
            } else {
                $ret = false;
            }
            return $ret;
        }
        
        
        // vrati true pokud je objednavka v nekterem z uzavrenych stavu
        public function isCompleted()
        {
            if (in_array($this->getStatus(), $this::STATUSES_COMPLETED)) {
                $ret = true;
            } else {
                $ret = false;
            }
            return $ret;
        }
                
                
        // vrati true pokud objednavka obsahuje dopravne
        public function shippingIncluded()
        {
            $ret = false;
            foreach ($this->getAllItems() AS $item) {
                if ($item->isShipping()) {
                    $ret = true;
                    break;
                }
            }
            return $ret;
        }
        
        
        // odesle objednavku tj. ulozi novou objednavku do db
        public function submit($finalMenucode)
        {
            // pokud je stav objednavky concept, pak ji muzeme odeslat
            if ($this->getStatus() == $this::STATUS_CONCEPT) {
                // vygenerujeme kod objednavky
                $this->setCode($this->genCode());
                // nastavime id bioplynky
                $this->setCustomerId($this->system->priv->getUserIdBiogas());
                // zmenime stav objednavky a posleme maily, vygenerujeme pdf
                $this->changeStatus($this->getStatusAfterSubmit());
                // ulozime do db
                $this->saveTodb();
                // posleme mail s objednavkou
                $this->sendEmail();
                // pokud mame updateDelivery, tak provedeme update adresare
                if ($this->getAddressUpdate()) {
                    $this->getCustomer()->setAddressDelivery($this->getAddressDelivery());
                    $this->getCustomer()->saveToDb();
                }

                // vymazeme ze session
                $this->clearSession();
                // presmerujeme na finalni obrazovku pokladny
                $this->redirectToFinal($finalMenucode);
            } else {
                throw new Exception ($this->system->getMessage('error_send_order_duplicate'));
            }
        }
        
        
        // vrati status pro novou objednavku podle toho, jestli se jedna o objednavku nebo poptavku
        private function getStatusAfterSubmit()
        {
            if ($this->isDemand()) {
                $status = $this::STATUSES_DEMAND['pending'];
            } else {
                $status = $this::STATUSES_ORDER['pending'];
            }
            
            return $status;
        }
        
        
        // zmeni stav objednavky a vrati, jestli doslo ke zmene stavu nebo ne
        public function changeStatus($newStatus)
        {
            $statusChanged = false;
            if ($newStatus != $this->getStatus()) {
                $this->setStatus($newStatus);
                $statusChanged = true;
            }
            
            return $statusChanged;
        }
        
        
        // odesle email
        public function sendEmail($emailTextAdd = '')
        {
            $recipients = array($this->getContactPerson()['email'] => $this->getContactPerson()['name']);
            $recipientsBcc = array('vl.brodsky@gmail.com' => 'Vladimír Brodský','chladek@salixus.cz'=>'Lukáš Chládek');
            $attachments = array('order.pdf' => $this->genPdf());
            list('subject' => $subject, 'body' => $body) = $this->emailPrepareTexts($emailTextAdd);
            $this->system->sendMail($subject, $recipients, $recipientsBcc, $body, $attachments);
        }
        
        
        // pripravi text emailu s objednavkou
        private function emailPrepareTexts($emailTextAdd = '')
        {
            // vycteni sablony mailu
            $emailTemplate = file_get_contents('templates/emails/order.html');
            // jazykova verze mailu
            $customer = new customer($this->system);
            $customer->loadFromDb($this->getCustomerId());
            $lang = $customer->getLangEmail();
            // data pro nahrazovani znacek
            $status = $this->getStatus();
            $data = $this->prepareDataForTemplate();
            // pokud mame url pro sledovani zasilky, tak ji doplnime do prislusne hlasky
            if ($data['shippingUrl']) {
                $data['shippingCheck'] = $this->system->replaceMarks($this->system->getMessage('shop_email_text_'.$status.'_check', $lang), $data);
            }
            // pripravime jednotlive casti mailu
            $emailSubject = $this->system->replaceMarks($this->system->getMessage('shop_email_subject_'.$status, $lang), $data, true);
            $emailText = '<p>'.$this->system->replaceMarks($this->system->getMessage('shop_email_text_'.$status, $lang), $data).'</p>';

            $textFooter = '<p>'.$this->system->getMessage('str_best_regards', $lang).'<br>'.$this->system->getMessage('str_biogest_team', $lang).'</p>';
            if ($emailTextAdd) {
                $textNote = '<p>'.$emailTextAdd.'</p>';
            } else {
                $textNote = '';
            }
            
            // nahradime znacky
            $data = array('textMain' => $emailText, 'textNote' => $textNote, 'textFooter' => $textFooter, 'url' => MAIL_SMTP['url']);
            $emailBody = $this->system->clearMarks($this->system->replaceMarks($emailTemplate, $data));
        
            return array('subject' => $emailSubject, 'body' => $emailBody);
        }
        


        // presmeruje na url s finalni obrazovkou pokladny
        public function redirectToFinal($finalMenucode)
        {
            $this->system->url->redirectMenuCode($finalMenucode);
        }
        
        
        // nastavi data podle pole ze session nebo z db
        public function setDataFromArr($data)
        {
            if (!empty($data) && is_array($data)) {
                $items = $data['items'];
                if (!empty($items) && is_array($items)) {
                    foreach ($items AS $itemData) {
                        $item = $this->addItem($itemData['code'], $itemData['quantity']);
                        if ($item) {
                            $item->setPrice((double)$itemData['price']);
                            $item->setPriceProduct((double)$itemData['priceOriginal']);
                            $item->setPriceTotal((double)$itemData['priceTotal']);
                            $item->setName($itemData['name']);
                            $item->setNameBase($itemData['nameBase']);
                            $item->setCodeOriginal($itemData['codeOriginal']);
                        }
                    }
                }
            }

            if (isset($data['discount'])) {
                $this->setDiscount($data['discount']);
            }
            if (isset($data['discountCode'])) {
                $this->setDiscountCode($data['discountCode']);
            }
            if (isset($data['addressBilling'])) {
                $this->setAddressBilling($data['addressBilling']);
            }
            if (isset($data['addressDelivery'])) {
                $this->setAddressDelivery($data['addressDelivery']);
            }
            if (isset($data['contactPerson'])) {
                $this->setContactPerson($data['contactPerson']);
            }
            if (isset($data['addressIsSame'])) {
                $this->setAddressIsSame($data['addressIsSame']);
            }
            if (isset($data['sendTogether'])) {
                $this->setSendTogether($data['sendTogether']);
            }
            if (isset($data['addressUpdate'])) {
                $this->setAddressUpdate($data['addressUpdate']);
            }
            if (isset($data['comment'])) {
                $this->setComment($data['comment']);
            }
            if (isset($data['commentEshop'])) {
                $this->setCommentEshop($data['commentEshop']);
            }
            if (isset($data['shippingUrl'])) {
                $this->setShippingUrl($data['shippingUrl']);
            }
            if (isset($data['emailTexts'])) {
                $this->setEmailTexts($data['emailTexts']);
            }
        }
        
        
        // pripravi data pro ulozeni do db i do session
        public function prepareDataForSave()
        {
            $data = array();
            $items = array();
            foreach ($this->getAllItems() AS $productCode => $item) {
                // ulozime pouze produkty, ktere maji pocet kusu vetsi nez 0
                if ($item->getQuantity() > 0) {
                    $items[] = array('code' => $productCode,
                                     'codeOriginal' => $item->getCodeOriginal(),
                                     'name' => $item->getName(),
                                     'nameBase' => $item->getNameBase(),
                                     'quantity' => $item->getQuantity(), 
                                     'price' => (string)$item->getPrice(),
                                     'priceTotal' => (string)$item->getPriceTotal(),
                                     'priceOriginal' => (string)$item->getPriceProduct(),
                                     );
                }
            }
            $data['items'] = $items;
            if (($value = $this->getDiscount()) !== false) {
                $data['discount'] = $value;
            }
            if (($value = $this->getDiscountCode()) !== false) {
                $data['discountCode'] = $value;
            }
            if (($value = $this->getAddressBilling()) !== false) {
                $data['addressBilling'] = $value;
            }
            if (($value = $this->getAddressDelivery()) !== false) {
                $data['addressDelivery'] = $value;
            }
            if (($value = $this->getContactPerson()) !== false) {
                $data['contactPerson'] = $value;
            }
            if (($value = $this->getAddressIsSame()) !== 0) {
                $data['addressIsSame'] = $value;
            }
            if (($value = $this->getSendTogether()) !== 0) {
                $data['sendTogether'] = $value;
            }
            if (($value = $this->getAddressUpdate()) !== 0) {
                $data['addressUpdate'] = $value;
            }
            if (($value = $this->getComment()) !== false) {
                $data['comment'] = $value;
            }
            if (($value = $this->getCommentEshop()) !== false) {
                $data['commentEshop'] = $value;
            }
            if (($value = $this->getShippingUrl()) !== false) {
                $data['shippingUrl'] = $value;
            }
            if (($value = $this->getEmailTexts()) !== false) {
                $data['emailTexts'] = $value;
            }
                        
            return $data;
        }


        // pripravime data pro nahrazeni znacek v templatech
        private function prepareDataForTemplate()
        {
            $data = $this->prepareDataForSave();
            // pokud je dodaci stejna jako fakturacni, tak adresu prekopirujeme
            if ($this->getAddressIsSame()) {
                $data['addressDelivery'] = $data['addressBilling'];
            }
            $data['orderCode'] = $this->getCode();
            $data['orderPriceTotal'] = formatPrice($this->getPrice());
            $data['orderDate'] = date('j.n.Y', $this->getDateOrigin());
            $data['emailAddress'] = SHOP_MAIL_ADDRESS;
            $data['shippingUrl'] = $this->getShippingUrl();

            return $data;
        }


        // nastavi vsechna data z postu
        public function setDataFromPost($data)
        {
            $this->setContactPerson(array('name' => (string)$data['contact'],
                                           'phone' => (string)$data['phone'],
                                           'email' => (string)$data['email'],
                                    ));
            $this->setAddressDelivery(array('name' => (string)$data['firm'],
                                             'street' => (string)$data['street'],
                                             'city' => (string)$data['city'],
                                             'zip' => (string)$data['zip'],
                                             'county' => (string)$data['county'],
                                             'country' => (string)$data['country'],
                                    ));
            $this->setAddressIsSame($data['sameDelivery'] ? true : false);
            $this->setSendTogether($data['sendTogether'] ? true : false);
            if (isset($data['deliveryUpdate'])) {
                $this->setAddressUpdate($data['deliveryUpdate'] ? true : false);
            }
            $this->setComment((string)$data['comment']);
            $this->setCommentEshop((string)$data['commentEshop']);
            $this->setShippingUrl((string)$data['shippingUrl']);
            $this->addEmailText((string)$data['emailText']);
            if ($data['discountCode']) {
                $this->setDiscountCode((string)$data['discountCode']);
            }
            foreach ($data['code'] AS $key => $code) {
                $item = $this->setQuantity((string)$code, (integer)$data['quantity'][$key]);
                $item->setPrice((double)$data['price'][$key]);
            }
            $this->recalculatePrice();
        }


        // zkontroluje nastavena data
        public function validateData($validateDiscount = true)
        {
            $errorArr = array();
            
            if ($validateDiscount) {
                // pokud je slevovy kod neplatny, tak neodesleme objednavku, ale vypiseme chybovou hlasku
                $discountCode = $this->getDiscountCode();
                if ($discountCode && !$this->discountIsValid($discountCode)) {
                    $errorArr[] = $this->system->getMessage('str_discount_code_error_not_valid');
                }
                // prepocitame ceny
                $this->discountApply($discountCode);
            }
                        
            // kontrola vyplneni vsech povinnych udaju
            $contactPerson = $this->getContactPerson();
            $address = $this->getAddressDelivery();
            if (!($contactPerson['name'] && $contactPerson['phone'] && $contactPerson['email']  
                && ($this->getAddressIsSame() || ($address['name'] && $address['street'] && $address['city'] && $address['zip'] && $address['country']))
               )) 
            {
                $errorArr[] = $this->system->getMessage('str_error_all_required');
            }
            
            return $errorArr;
        }
    
    
        // nacte bioplynku
        public function loadCustomer($customerId = false)
        {
            if (!$customerId) {
                $customerId = $this->system->priv->getUserIdBiogas();
            }
            $customer = new customer($this->system);
            $customer->loadFromDb($customerId);
            $this->setCustomer($customer);
            $this->setAddressBilling($customer->getAddressBilling());
            $this->setAddressDelivery($customer->getAddressDelivery());
            $this->setContactPerson(array('name' => $this->system->priv->getUserName(), 
                                          'phone' => $this->system->priv->getUserPhone(), 
                                          'email' => $this->system->priv->getUserEmail()));
            // pokud neco neni vyplneno v dodaci adrese, tak defaultne zaskrtneme, ze je dodaci stejna jako fakturacni
            $address = $this->getAddressDelivery();
            if ($address['name'] && $address['street'] && $address['city'] && $address['zip'] && $address['country']) {
                $addressIsSame = false;
            } else {
                $addressIsSame = true;
            }
            $this->setAddressIsSame($addressIsSame);
            $this->setAddressUpdate($addressIsSame);
        }


        // vytvori novy objekt s objednavkou a presune na nej zadane polozky
        public function divideItems($itemsCodes)
        {
            if (is_array($itemsCodes) && !empty($itemsCodes)) {
                $orderCopy = clone $this;
                $orderCopy->setId(null);
                $orderCopy->setCode($orderCopy->genDividedCode());
                $orderCopy->removeAllItems();
                // presuneme itemy z puvodni objednavky na novou
                foreach ($itemsCodes AS $code) {
                    $orderCopy->setItem($code, $this->getItem($code));
                    $this->removeItem($code);
                }
            }
            
            return $orderCopy;
        }
        
        
        // vygeneruje novy kod pro objednavku
        private function genCode()
        {
            return $this->getCustomer()->getCode().'-'.date('Ymd-His');
        }

        
        // vygeneruje novy kod pro rozdelenou objednavku
        private function genDividedCode()
        {
            $originalCode = $this->getCode();
            $sql = "SELECT id FROM eshop_order o WHERE code = ?";
            $stmt = $this->system->db->prepare($sql);

            // zvysujeme index rozdelene objednavky a overime ho v db. V pripade ze existuje, tak index zvysime
            $index = 1;
            while ($index < 100) {
                $code = $originalCode.'-'.$index;
                // pripravime data
                $stmt->bind_param('s', $code);
                // spustime pripraveny dotaz
                $stmt->execute();
                // vycteme vysledek
                $result = $stmt->get_result();
                if ($result->num_rows == 0) {
                    break;
                }
                $index++;
            }
            if ($index >= 100) {
                throw new Exception ($this->system->getMessage('str_order_can_not_devided'));
            }
            return $code;
        }
                
        
        // vygeneruje pdf a posleme na prislusne destination
        public function genPdf($pdfDestination = 'STRING_RETURN', $filename = '')
        {
            // pripravime si data pro nahrazeni znacek v templatech
            $data = $this->prepareDataForTemplate();
            
            // nahradime znacky v jednotlivych templatech
            $headerHtml = $this->system->replaceMarks(file_get_contents('templates/pdf/order/head.html'), $data);
            $footerHtml = $this->system->replaceMarks(file_get_contents('templates/pdf/order/footer.html'), $data);
            $itemsHtml = '';
            foreach ($data['items'] AS $itemData) {
                $item = $this->getItem($itemData['code']);
                $itemData['price'] = formatPrice($itemData['price'], '', $item->isDemand(), $item->isShipping());
                $itemData['priceOriginal'] = formatPrice($itemData['priceOriginal'], '', $item->isDemand(), $item->isShipping());
                $itemData['priceTotal'] = formatPrice($itemData['priceTotal'], '', $item->isDemand(), $item->isShipping());
                $itemsHtml .= $this->system->replaceMarks(file_get_contents('templates/pdf/order/item.html'), $itemData);
            }
            $data['orderItems'] = $itemsHtml;
            $bodyHtml = $this->system->clearMarks($this->system->replaceMarks(file_get_contents('templates/pdf/order/body.html'), $data));
            
            require_once (GLOBAL_PATH.'/library/mpdf/vendor/autoload.php');

            $mpdf = new \Mpdf\Mpdf([
            	'mode' => 's',
            	'margin_left' => 10,
            	'margin_right' => 10,
            	'margin_top' => 65,
            	'margin_bottom' => 45,
            	'margin_header' => 10,
            	'margin_footer' => 10 
            ]);
            $mpdf->SetHTMLHeader($headerHtml);
            $mpdf->SetHTMLFooter($footerHtml);
            $mpdf->WriteHTML($bodyHtml);
            

            return $mpdf->Output($filename, constant('\Mpdf\Output\Destination::'.$pdfDestination)); 
        }




                

        // uplatneni zadane slevy
        public function discountApply($discountCode) 
        {
            $ret = false;
            // slevu uplatnime pouze pokud jeste nebyla uplatnena
            if (!$this->discountIsApplied()) {
                // overeni slevoveho kodu v adresari
                if ($this->discountIsValid($discountCode)) {
                    $discount = $this->getDiscountRate();
                    $this->setDiscountCode($discountCode);
                    $this->setDiscount($discount);
                    $this->discountUpdateItemsPrices($discount);
                    $ret = true;
                }
            } else {
            // pokud byla uplatnena tak overime platnost slevy a pripadne ji zrusime a prepocteme
                if (!$this->discountIsValid($discountCode)) {
                    $discount = 0;
                    $this->setDiscount($discount);
                    $this->setDiscountCode('');
                    $this->discountUpdateItemsPrices($discount);
                }
            }
            
            return $ret;
        }

        
        // prepocitani cen polozek pri pouziti slevy
        private function discountUpdateItemsPrices($discount)
        {
            foreach ($this->getAllItems() AS $item) {
                // zapocitani slevy
                $item->discount($discount);
            }
            $this->recalculatePrice();
        }
        

        // kontrola jestli jiz byla uplatnena sleva
        public function discountIsApplied() 
        {
            $ret = false;
            if ($this->getDiscount()) {
                $ret = true;
            }
            return $ret;
        }
        
        
        // kontrola jestli zadany kod je platny
        public function discountIsValid($discountCode) 
        {
            $ret = false;
            if ($discountCode && $this->getCustomer()->getValidDiscountCode() === $discountCode) {
                $ret = true;
            }
            return $ret;
        }



            
    
        
            
        // prida konkretni produkt na objednavku. Pokud uz je v objednavce, tak zvysi pocet kusu o jedna
        public function addItem($productCode, $quantity = 1)
        {
            // produkt uz je na objednavce, tak pridame mnozstvi
            if ($item = $this->getItem($productCode)) {
                $quantity = $item->getQuantity() + $quantity;
            } else {
            // pokud zatim nebyl na objednavce, tak ho vytvorime
                $item = $this->createItem($productCode);
            }
            if ($item) {
                $item->setQuantity($quantity);
            }
            
            return $item;
        }
        
        
        // odstranime vsechny produkty z objednavky
        public function removeAllItems()
        {
            foreach ($this->getAllItems() AS $productCode => $item) {
                $this->removeItem($productCode);
            }
            $this->recalculatePrice();
        }

    
        // odstranime produkt z objednavky
        public function removeItem($productCode)
        {
            unset($this->items[$productCode]);
            $this->recalculatePrice();
        }
    

        // zmenime pocet kusu produktu na objednavce
        public function setQuantity($productCode, $quantity)
        {
            // pokud produkt neni na objednavce, tak ho vytvorime
            if (!$item = $this->getItem($productCode)) {
                $item = $this->createItem($productCode);
            }
            if ($item) {
                // nastavime pozadovane mnozstvi nebo produkt odebereme
                if ($quantity > 0) {
                    $item->setQuantity($quantity);
                } else {
                    $this->removeItem($productCode);
                }
                $this->recalculatePrice();
            }
            
            return $item;
        }

        
        // vytvori polozku na objednavce
        private function createItem($productCode)
        {
            $item = new shopOrderItem($this->system, $productCode);
            // pokud neexistuje produkt na itemu, tak vratime false
            if ($item->productExist()) {
                $item->discount($this->getDiscount());
                $this->items[$productCode] = $item;
            } else {
                $item = false;
            }
            return $item;
        }
                
        
        // vrati jeden konkretni item z objednavky
        public function getItem($productCode)
        {
            $ret = false;
            if ($item = $this->items[$productCode]) {
                $ret = $item;
            }
            return $ret;       
        }        

    
        // nastavi jeden konkretni item z objednavky
        public function setItem($productCode, $item)
        {
            $this->items[$productCode] = $item;
            $this->recalculatePrice();
        }        


        // vrati vsechny itemy na objednavce
        public function getAllItems()
        {
            return $this->items;        
        }
        
            
        // vrati celkovy pocet produktu v objednavce
        public function getItemsQuantity()
        {
            $quantity = 0;
            foreach ($this->getAllItems() AS $item) {
                $quantity += $item->getQuantity();
            }
        
            return $quantity;
        }   
        
                
        // vrati true, pokud ma uzivatel pravo pristupu k objednavce
        public function userHasAccess()
        {
            $ret = false;
            if ($this->system->priv->userIsAdmin() || $this->system->priv->getUserIdBiogas() == $this->getCustomerId()) {
                $ret = true;
            }
        
            return $ret;
        }
        
                

        // vrati data z db
        public function getDbData()
        {
            return $this->dbData;
        } 


        // nastavi data vyctena z db
        public function setDbData($value)
        {
            $this->dbData = (array)$value;
        }


        // vrati vysku slevy v procenteh
        public function getDiscountRate() 
        {
            return $this->getCustomer()->getDiscountRate();
        }
        
    
        // vrati slevu
        public function getDiscount()
        {
            return $this->discount;
        }
    
    
        // nastavi slevu
        public function setDiscount($discount)
        {
            $this->discount = $discount;
        }


        // vrati slevovy kod
        public function getDiscountCode()
        {
            return $this->discountCode;
        }
    
    
        // nastavi slevovy kod
        public function setDiscountCode($discountCode)
        {
            $this->discountCode = $discountCode;
        }

                
        // vrati id v db
        public function getId()
        {
            return $this->id;
        } 


        // nastavi id objednavky
        public function setId($value)
        {
            $this->id = $value;
        }


        // vrati code objednavky
        public function getCode()
        {
            return $this->code;
        } 

                
        // nastavi kod objednavky
        public function setCode($value)
        {
            $this->code = $value;
        }


        // vrati datum objednavky
        public function getDateOrigin()
        {
            return $this->dateOrigin;
        } 

                
        // nastavi datum objednavky
        public function setDateOrigin($value)
        {
            $this->dateOrigin = $value;
        }


        // vrati datum zmeny objednavky
        public function getDateChange()
        {
            return $this->dateChange;
        } 


        // nastavi datum zmeny objednavky
        public function setDateChange($value)
        {
            $this->dateChange = $value;
        }


        // vrati id bioplynky na objednavce
        public function getCustomerId()
        {
            return $this->customerId;
        }
                        

        // nastavi id bioplynky na objednavce
        public function setCustomerId($value)
        {
            $this->customerId = $value;
        }


        // vrati bioplynku na objednavce
        public function getCustomer()
        {
            return $this->customer;
        }
                        

        // nastavi bioplynku na objednavce
        public function setCustomer($value)
        {
            $this->customer = $value;
        }


        // vrati cenu na objednavce
        public function getPrice()
        {
            if ($this->price == false) {
                $this->recalculatePrice();
            }
        
            return $this->price;
        }


        // spocita cenu na objednavce
        private function recalculatePrice()
        {
            $price = 0;
            foreach ($this->getAllItems() AS $item) {
                $price += $item->getPriceTotal();
            }
            $this->setPrice($price);
        }


        // nastavi cenu na objednavce
        public function setPrice($value)
        {
            $this->price = $value;
        }


        // vrati status objednavky
        public function getStatus()
        {
            return $this->status;
        }


        // nastavi status objednavky
        public function setStatus($value)
        {
            $this->status = $value;
        }

        
        // vrati dorucovaci adresu
        public function getAddressDelivery()
        {
            return $this->addressDelivery;
        }
        

        // vrati fakturacni adresu
        public function getAddressBilling()
        {
            return $this->addressBilling;
        }
        

        // vrati kontaktni osobu
        public function getContactPerson()
        {
            return $this->contactPerson;
        }
        
        // vrati dodaci adresa je stejna jako fakturacni
        public function getAddressIsSame()
        {
            return $this->addressIsSame;
        }
        

        // vrati dorucit vse spolecne
        public function getSendTogether()
        {
            return $this->sendTogether;
        }
        

        // vrati aktualizaci dodaci adresy
        public function getAddressUpdate()
        {
            return $this->addressUpdate;
        }
        

        // vrati poznamku
        public function getComment()
        {
            return $this->comment;
        }                                


        // nastavi dorucovaci adresu
        public function setAddressDelivery($address)
        {
            $this->addressDelivery = $address;
        }
        

        // nastavi fakturacni adresu
        public function setAddressBilling($address)
        {
            $this->addressBilling = $address;
        }        
        

        // nastavi kontaktni osobu
        public function setContactPerson($contact)
        {
            $this->contactPerson = $contact;
        }
        

        // nastavi dodaci adresa je stejna jako fakturacni
        public function setAddressIsSame($value)
        {
            $this->addressIsSame = $value;
        }
        

        // nastavi dorucit vse spolecne
        public function setSendTogether($value)
        {
            $this->sendTogether = $value;
        }
        

        // nastavi aktualizaci dodaci adresy
        public function setAddressUpdate($value)
        {
            $this->addressUpdate = $value;
        }
        

        // nastavi poznamku
        public function setComment($value)
        {
            $this->comment = $value;
        }                                


        // vrati url pro sledovani zasilky
        public function getShippingUrl()
        {
            return $this->shippingUrl;
        }


        // nastavi url pro sledovani zasilky
        public function setShippingUrl($value)
        {
            $this->shippingUrl = $value;
        }
        
        
        // vrati poznamku eshopu
        public function getCommentEshop()
        {
            return $this->commentEshop;
        }         

        
        // nastavi poznamku eshopu
        public function setCommentEshop($value)
        {
            $this->commentEshop = $value;
        }         


        // vrati texty mailu
        public function getEmailTexts()
        {
            return $this->emailTexts;
        }         

        
        // nastavi texty mailu
        public function setEmailTexts($value)
        {
            $this->emailTexts = $value;
        }         

        
        // prida novy text mailu
        public function addEmailText($value)
        {
            if ($value) {
                if (empty($emailTexts = $this->getEmailTexts())) {
                    $emailTexts = array();
                }
                $emailTexts[] = $value;
                $this->setEmailTexts($emailTexts);
            }
        }         



            
    }
    
    