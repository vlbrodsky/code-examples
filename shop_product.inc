<?php  
    
    // trida pro jednotlive produkty
    class shopProduct 
    {
        // hlavni objekt
        private $system;
        
        // kod produktu
        private $code;        
        
        // kod produktu original biogest
        private $codeOriginal; 

        // hlavni nazev produktu
        private $name = '';
                        
        // pole s nazvy v jednotlivych jazycich
        private $namesLang = array();
        
        // popisy v jednotlivych jazycich
        private $descriptionsLang = array();
        
        // cena
        private $price; 
        
        // hlavni kategorie
        private $meuCode; 

        // datum zmeny
        private $dateChange;
                
        // data vyctena z db
        public $dbData = array();
        
        // cesty k obrazkum
        private $imagePaths = array('normal' => 'img/product/normal/', 'big' => 'img/product/big/');


         // konstruktor
        public function __construct($system)
        {
            $this->system = $system;
        }


        // nacte produkt z databaze - pouze data
        public function loadDataFromDb($sqlWhereArr, $sqlOrder = false)
        {   
            $productsArr = array();
            
            if (!is_array($sqlWhereArr)) {
                $sqlWhereArr = array();
            }

            if ($sqlOrder) {
                $sqlOrder = ' ORDER BY '.$sqlOrder;
            } else {
                $sqlOrder = '';
            }

            // pridame obecne podminky
            $sqlWhereArr[] = 'deleted = 0';
            
            $sql = "SELECT code, code_original, name, name_cz, name_de, name_en, name_it, name_fr, name_rs, 
                            description_cz, description_de, description_en, description_it, description_fr, description_rs, price, date_change,  
                            (SELECT menu_code FROM eshop_product_menu m WHERE m.product_code=p.code LIMIT 1) AS menu_code
                        FROM eshop_product p
                        WHERE ".join(' AND ', $sqlWhereArr).$sqlOrder;
//                         _D($sql, 1);

            $res = $this->system->db->query($sql);
            while ($row = $res->fetch_assoc()) {
                $productsArr[$row['code']] = $row;
            }
            
            return $productsArr;
        }


        // nastavi data objednavky podle dat vyctenych z db
        public function setFromDbRow($dataRow)
        {
            $this->setCode($dataRow['code']);
            $this->setCodeOriginal($dataRow['code_original']);
            $this->setNameBase($dataRow['name']);
            $this->setDateChange($dataRow['date_change']);
            $this->setMenuCode($dataRow['menu_code']);
            $this->setPrice($dataRow['price']);
            $this->setNamesLang(array('default' => $dataRow['name_de'], 
                                     'cz' => $dataRow['name_cz'],
                                     'de' => $dataRow['name_de'],
                                     'en' => $dataRow['name_en'],
                                     'it' => $dataRow['name_it'],
                                     'fr' => $dataRow['name_fr'],
                                     'rs' => $dataRow['name_rs'],
                                     ));
            $this->setDescriptionsLang(array('default' => $dataRow['description_de'], 
                                             'cz' => $dataRow['description_cz'],
                                             'de' => $dataRow['description_de'],
                                             'en' => $dataRow['description_en'],
                                             'it' => $dataRow['description_it'],
                                             'fr' => $dataRow['description_fr'],
                                             'rs' => $dataRow['description_rs'],
                                             ));
            $this->setDbData($dataRow);
        }


        
        // nastavi kod produktu
        public function setCode($value)
        {
            $this->code = $value;
        }


        // vrati kod produktu
        public function getCode()
        {
            return $this->code;
        }


        // vrati kod produktu, ktery byl upraven tak, aby mohl byt pouzit v javascriptu
        public function getCodeJsEscaped()
        {
            return strtr($this->code, '.+-*/();"', '_________');
        }
                
        
        // nastavi originalni biogest kod
        public function setCodeOriginal($value)
        {
            $this->codeOriginal = $value;
        }


        // vrati originalni biogest kod
        public function getCodeOriginal()
        {
            return $this->codeOriginal;
        }
        
        
        // nastavi polozku menu, ve ktere je produkt zarazen
        public function setMenuCode($value)
        {
            $this->menuCode = $value;
        }
        

        // vrati polozku menu, ve ktere je produkt zarazen
        public function getMenuCode()
        {
            return $this->menuCode;
        }
        

        // nastavi datum změny
        public function setDateChange($value)
        {
            $this->dateChange = $value;
        }        


        // vrati datum změny
        public function getDateChange()
        {
            return $this->dateChange;
        }        
        
        
        // nastavi cenu
        public function setPrice($value)
        {
            $this->price = $value;
        }
        

        // vrati cenu
        public function getPrice()
        {
            return round($this->price, 2);
        }
                        
                       
        // nastavi hlavni nazev
        public function setNameBase($value)
        {
            $this->name = $value;
        }        


        // vrati hlavni nazev
        public function getNameBase()
        {
            return $this->name;
        }        


        // nastavi vsechny nazvy
        public function setNamesLang($value)
        {
            $this->namesLang = $value;
        }        


        // vrati nazev v dane jazykove verzi
        public function getName($lang = false)
        {
            if (!$lang) {
                $lang = $this->system->getLang();
            }
            return ($this->namesLang[$lang] ? $this->namesLang[$lang] : $this->namesLang['default']);
        }   
        

        // nastavi vsechny popisy
        public function setDescriptionsLang($value)
        {
            $this->descriptionsLang = $value;
        }        


        // vrati popis v dane jazykove verzi
        public function getDescription($lang = false)
        {
            if (!$lang) {
                $lang = $this->system->getLang();
            }
            return ($this->descriptionsLang[$lang] ? $this->descriptionsLang[$lang] : $this->descriptionsLang['default']);
        }
                
        
        // vrati url
        public function getUrl($params = false)
        {
            if (empty($params)) {
                $params = array();
            }
            // pridame parametr pro kod produktu
            $params[$this->system->url::PARAM_MENU_CODE] = $this->getMenuCode();
            $params[$this->system->url::PARAM_PRODUCT_CODE] = $this->getCode();
            
            return $this->system->url->getUrl($params, false, $this->system->menu->getItem($this->getMenuCode()), $this);
        }        
        
        
        // vrati hlavni obrazek produktu ve forme pole
        public function getPicture()
        {
            $ret = array();
            foreach ($this->imagePaths AS $pathType => $path) {
                $filePath = $path.$this->getCode().'.jpg';
                if (is_file($filePath)) {
                    $ret[$pathType] = $filePath;
                }
            }
        
            return $ret;
        }
        
        
        // vrati pole vsech obrazku produktu
        public function getAllPictures()
        {
            // jako prvni obrazek vlozime hlavni obrazek produktu
            $ret = array($this->getCode().'.jpg' => $this->getPicture());
            
            // pridame ostatni obrazky
            foreach ($this->imagePaths AS $pathType => $path) {
                foreach (glob($path.$this->getCode().'_*.jpg') as $filename) {
                    $ret[basename($filename)][$pathType] = $filename;
                }
            }
        
            return $ret;
        }
        
        
        // vrati true pokud se jedna o poptavku tj. produkt ma nulovou cenu a nejedna se o dopravne
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
            if ($this->getCode() == SHOP_SHIPPING) {
                $ret = true;
            } else {
                $ret = false;
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
        
        
        
        
        
        
    }    
    