<?php

    include_once(GLOBAL_PATH.'/library/shop_product.inc');
    include_once(GLOBAL_PATH.'/library/shop_order.inc');


    // trida pro shop
    class shop 
    {
        // hlavni objekt
        private $system;
        
        // pole s produkty
        private $products;
        
        // aktualni produkt
        private $actualCode;        
                
         // konstruktor
        public function __construct($system)
        {
            $this->system = $system;
            $this->products = array();
            $this->actualCode = (string)$this->system->url->getParam($this->system->url::PARAM_PRODUCT_CODE);
            $this->currency = 'EUR';
        }
        
                
        // vrati produkty, ktere se nachazeji v prislusne nabidce nebo v celem podstromu dane kategorie
        public function getProductsInOffer($sqlOrder = 'code', $flag = false, $subtree = false)
        {
            $sqlWhereArr = array();
            // pokud mame zadany flag, tak omezime produkty pouze na dany flag
            if ($flag) {
                 $sqlWhereArr[] = "flags LIKE '%".$flag."%'";
            } else {
                // vzdy pouzijeme aktualni polozku menu
                $menuCodes = array($this->system->menu->getActualCode());
                // v pripade, ze chceme produkty pro cely podstrom, tak pridame i kody podpolozek
                if ($subtree) {
                    $subtreeItems = $this->system->menu->getSubtree($this->system->menu->getActualCode());
                    foreach ($subtreeItems AS $item) {
                        $menuCodes[] = $item->getCode();
                    }
                }
                $sqlWhereArr[] = "EXISTS (SELECT menu_code FROM eshop_product_menu m 
                                            WHERE m.product_code=p.code AND m.menu_code IN ('".join("','", $menuCodes)."'))";
            }
            
            // nacteme produkty z db
            $productsArr = $this->loadProductsFromDb($sqlWhereArr, $sqlOrder);

            return $productsArr;
        }
        

        // vrati produkt, ktere se ma zobrazit v detailu
        public function getProductInDetail($productCode = false)
        {
            $productCode = $productCode ? $productCode : $this->getActualCode();
            return $this->getProduct($productCode);
        }
        
        
        // vrati pozadovany produkt
        public function getProduct($productCode, $codeOriginal = false)
        {
            
            $sqlWhereArr = array(($codeOriginal ? "code_original" : "code")." = '".$productCode."'");
            // nacteme produkty z db
            $productsArr = $this->loadProductsFromDb($sqlWhereArr);
            // vratime prvni produkt
            $product = reset($productsArr);

            return $product;
        }
        
        
        // nacte produkty z db a vytvori prislusne objekty
        public function loadProductsFromDb($sqlWhereArr, $sqlOrder = false)
        {
            $productsResult = array();
    
            $product = new shopProduct($this->system);
            $productsResultData = $product->loadDataFromDb($sqlWhereArr, $sqlOrder);
            unset($product);        
    
            foreach ($productsResultData AS $row) {
                $product = new shopProduct($this->system);
                $product->setFromDbRow($row);
                $productsResult[$product->getCode()] = $product;
            }
            
            return $productsResult;
        }

        
        // vrati jestli se ma zobrazit detail nebo offer
        public function isInDetail()
        {
            return $this->getActualCode() ? true : false;
        }   
     
        
        // vrati objekt s objednavkou nactenou z db
        public function getOrderFromDb($orderId, $history = false)
        {
            $order = new shopOrder($this->system, $this, $history);
            $order->loadFromDb($orderId);
            return $order;
        }   
        
                
        // vrati objekt s objednavkou nactenou ze session
        public function getOrderFromSession($loadCustomer = false)
        {
            $order = new shopOrder($this->system, $this);
            // nacteni bioplynky
            if ($loadCustomer) {
                $order->loadCustomer();                
            }
            $order->loadFromSession();
            return $order;
        }   
        
        
        // vycisti kosik v session
        public function clearOrderSession()
        {
            $order = new shopOrder($this->system, $this);
            $order->loadFromSession();
            $order->clearSession();
            $order->saveToSession();
        }
                
        
        // nacte objednavky z db
        public function loadOrdersFromDb($sqlWhereArr, $sqlOrder, $history)
        {
            $ordersResult = array();
    
            $order = new shopOrder($this->system, $this, $history);
            $ordersResultData = $order->loadDataFromDb($sqlWhereArr, $sqlOrder);
            unset($order);        
    
            foreach ($ordersResultData AS $row) {
                $order = new shopOrder($this->system, $this, $history);
                $order->setFromDbRow($row);
                $ordersResult[] = $order;
            }
            
            return $ordersResult;
        } 
    

        // vrati kod aktualniho produktu
        public function getActualCode()
        {
            return $this->actualCode;
        }        
        
        
        // vrati oznaceni meny
        public function getCurrency()
        {
            return $this->currency;
        } 
        
                
        
        
    }
    
    
    
    

    
    