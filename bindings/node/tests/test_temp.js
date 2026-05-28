#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{clade |style=font-size:90%; line-height:90%;
   |1=''[[Mimipiscis]]'' ([[Actinopterygii]])
   |2=''[[Porolepis]]'' ([[Porolepiformes]])
   |label3='''Actinistia'''
   |3={{clade
      |1=''[[Miguashaia]]''
      |2=''[[Styloichthys]]''
      |3=''[[Gavinia]]''
      |4={{clade
         |1=''[[Diplocercides]]''
         |2={{clade
            |1=''[[Serenichthys]]''
            |2={{clade
               |1=''[[Holopterygius]]''
               |2=''[[Allenypterus]]''
               }} 
            |3={{clade
               |1=''[[Lochmocercus]]''
               |2={{clade
                  |1={{clade
                     |1=''[[Rhabdoderma]]''
                     |2={{clade
                        |1=''[[Caridosuctor]]''
                        |2={{clade
                           |1={{clade
                              |1=''[[Sassenia]]''
                              |2=''[[Spermatodus]]''
                              }}
                           |2={{clade
                              |1={{clade
                                 |1=''[[Piveteauia]]''
                                 |2={{clade
                                    |1=''[[Coccoderma]]''
                                    |2=''[[Laugia]]''
                                    }} 
                                 }}
                              |2={{clade
                                 |1=''[[Coelacanthus]]''
                                 |2={{clade
                                    |1=''[[Guizhoucoelacanthus]]''
                                    |2={{clade
                                       |1={{clade
                                          |1=''[[Wimania]]''
                                          |2=''[[Axelia]]''
                                          }} 
                                       |2={{clade
                                          |1=''[[Whiteia]]''
                                          |2={{clade
                                             |1=''[[Heptanema]]''
                                             |2=''[[Dobrogeria]]''
                                             |label3=[[Latimerioidei]]
                                             |3={LATIMERIOIDEA}
                                             }} 
                                          }} 
                                       }} 
                                    }} 
                                 }} 
                              }} 
                           }} 
                        }} 
                     }} 
                  }} 
               }} 
            }} 
         }} 
      }}
}}`,
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
