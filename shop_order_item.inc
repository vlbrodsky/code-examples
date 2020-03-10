<?php

    // trida s polozkou na objednavce
    class shopOrderItem
    {
        // hlavni objekt
        private $system;
        
        // kod produktu
        private $code;
        
        // id itemu v db
        private $id;
        
        // samotny objekt s produktem
        private $product;
        
        // mnozstvi produktu v objednavce
        private $quantity;
        
        // cena produktu
        private $priceProduct;
                
        // cena produktu s pripadnou slevou
        private $price;

        // cena produktu za vsechny produkty
        private $priceTotal = false;

        // zakladni nazev produktu
        private $nameBase = false;

        // nazev produktu
        private $name = false;

        // zakladni kod produktu
        private $codeOriginal = false;


        // konstruktor - pripravi zakladni data
        public function __construct($system, $productCode)
        {
            $this->system = $system;
            $this->quantity = 1;
            $this->code = $productCode;
            $this->product = $this->system->shop->getProduct($productCode);
            if ($this->product) {
                $this->priceProduct = $this->product->getPrice();
                $this->price = $this->priceProduct;
            }
        }
        
        
        // vrati false, pokud se nepodarilo nacist produkt z db tj. neexistuje
        public function productExist()
        {
            if ($this->product) {
                $ret = true;
            } else {
                $ret = false;
            }
            
            return $ret;
        }
    
    
        // ulozi polozku objednavku do databaze
        public function saveToDb($orderId)
        {
            // ulozime pouze produkty, ktere maji pocet kusu vetsi nez 0
            if ($this->getQuantity() > 0) {
                $dataItem = array();
                $dataItem['id_user_change'] = $this->system->priv->getUserId();
                $dataItem['id_order'] = $orderId;
                $dataItem['code_product'] = $this->getCode();
                $dataItem['price'] = $this->getPrice();
                $dataItem['quantity'] = $this->getQuantity();
                $dataItem['price_total'] = $this->getPriceTotal();
                // ulozime do db
                saveDataToDb($this->system->db, 'eshop_order_item', $this->getId(), $dataItem);
            } else {
            // jinak smazeme z db
                $sql = "DELETE FROM eshop_order_item WHERE id_order=? AND code_product=?";
                $stmt = $this->system->db->prepare($sql);
                // pripravime data
                $idOrderParam = (integer)$orderId;
                $productCodeParam = (string)$this->getCode();
                $stmt->bind_param('is', $idOrderParam, $productCodeParam);
                // spustime pripraveny dotaz
                $stmt->execute();
            }
        }
    
    
        // vrati produkt odpovidajici polozce
        public function getProduct()
        {
            return $this->product;
        }
        
        
        // vrati true pokud se jedna o poptavku tj. produkt ma nulovou cenu
        public function isDemand()
        {
            if ($this->getPrice() > 0 || $this->isShipping()) {
                $ret = false;
            } else {
                $ret = true;
            }
            return $ret;
        }
        
        
        // vrati true pokud se jedna o polozku odpovidajici dopravnemu
        public function isShipping()
        {
            return $this->product->isShipping();
        }
        
        
        
        // zapocita slevu
        public function discount($discount)
        {
            if ($discount > 0) {
                $this->setPrice(round(($this->getPriceProduct() * (100 - $discount) / 100), 1));
            } else {
            // odstraneni slevy a nastaveni standardni ceny produktu
                $this->setPrice($this->getPriceProduct());
            }
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
        
                // nastavi cenu produktu na objednavce
        public function setPrice($price)
        {
            $this->price = (double)$price;
            $this->recalculatePriceTotal();
        }

        
        // vrati zakladni cenu produktu
        public function getPriceProduct()
        {
            return $this->priceProduct;
        }
        
        
        // nastavi zakladni cenu produktu
        public function setPriceProduct($value)
        {
            $this->priceProduct = $value;
        }
        

        // vrati cenu produktu na objednavce
        public function getPrice()
        {
            return $this->price;
        }
        
        
        // vrati cenu produktu na objednavce pro dany pocet kusu
        public function getPriceTotal()
        {
            if ($this->priceTotal == false) {
                $this->recalculatePriceTotal();
            }

            return $this->priceTotal;
        }
        
        
        // nastavi cenu produktu na objednavce pro dany pocet kusu
        public function setPriceTotal($value)
        {
            $this->priceTotal = $value;
        }
        

        // prepocita cenu produktu
        private function recalculatePriceTotal()
        {
            $this->setPriceTotal($this->getPrice() * $this->getQuantity());
        }
        
                            
        // vrati mnozstvi produktu na objednavce
        public function getQuantity()
        {
            return $this->quantity;
        }
                
            
        // nastavi mnozstvi produktu na objednavce
        public function setQuantity($value)
        {
            $this->quantity = $value;
            $this->recalculatePriceTotal();
        }
    
    
        // vrati nazev produktu
        public function getName()
        {
            if ($this->name === false) {
                $this->setName($this->product->getName());
            }
            return $this->name;
        }
                
            
        // nastavi nazev produktu
        public function setName($value)
        {
            $this->name = $value;
        }
        

        // vrati zakladni nazev produktu
        public function getNameBase()
        {
            if ($this->nameBase === false) {
                $this->setNameBase($this->product->getNameBase());
            }
            return $this->nameBase;
        }
                
            
        // nastavi nazev produktu
        public function setNameBase($value)
        {
            $this->nameBase = $value;
        }        


        // vrati kod produktu
        public function getCode()
        {
            return $this->code;
        }


        // vrati originalni kod produktu
        public function getCodeOriginal()
        {
            if ($this->codeOriginal === false) {
                $this->setCodeOriginal($this->product->getCodeOriginal());
            }
            return $this->codeOriginal;
        }
                
            
        // nastavi originalni kod produktu
        public function setCodeOriginal($value)
        {
            $this->codeOriginal = $value;
        }        



    
    }