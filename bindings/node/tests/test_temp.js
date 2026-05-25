#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `
{{Clade
 |style=font-size:80%; line-height:85%;background-color:#eeeeff
 |caption=Evolution of hyaenas
 |captionstyle=background-color:#8899ff;padding:10px;font-weight:bold;
 |footer=Phylogenic relationships based on morphological characteristics, after Werdelin & Solounias (1991) and Turner et al (2008)
 |footerstyle=background-color:#eeeeee;text-align:center;
 |1={{clade
  |9={{clade
    |label1=
    |1={{clade
        |label1=''[[Plioviverrops]]''
        |style1=background-color:#ccffcc;
        |1={{clade
           |grouplabel1=Civet/mongoose-like insectivore-omnivores 
           |grouplabelstyle1=font-size:12px;font-weight:bold;color:darkgreen;text-align:right;
           |label1=
           |1=''Plioviverrops gervaisi''
           |2={{clade
              |1=''Plioviverrops orbignyi''
              |2={{Clade
                 |label1=
                 |1=''Plioviverrops guerini''
                 |2=''Plioviverrops faventinus''
                 |3=''Plioviverrops gaudryi''
                  }} 
               }} 
            }}
         }}
     |2={{clade
        |label1=
        |1=''Tungurictis spocki''
        |2={{clade
           |label1=
           |1=''[[Thalassictis|Thalassictis robusta]]''
           |2=''"Thalassictis" certa''
           |3=''"Thalassictis" montadai''
           |4=''"Thalassictis" proava''
           |5=''"Thalassictis" sarmatica''
           |6=''"Thalassictis" spelaea''
           |7={{clade
              |label1=
              |1=''Tongxinictis primordialis''
              |2={{clade
                 |label1=
                 |1={{clade
                    |label1=''[[Proteles]]''
                    |style1=background-color:#ddeeff;
                    |1={{clade
                       |label1=
                       |1='''''[[Proteles cristatus]]''''' (aardwolf) [[File:The life of animals (Colored Plate 4) (proteles cristatus).jpg|50 px]]
                       |2=''Proteles amplidentus''
                        }}
                     }}
                 |2={{clade
                    |label1=
                    |1={{clade
                       |label1=''[[Ictitherium]]''
                       |style=background-color:#ddddff;
                    |2={{clade 
                       |label1=
                       |1=''Miohyaenotherium bessarabicum''
                       |2={{clade
                          |1={{clade
                             |label1=''[[Hyaenotherium]]''
                             |1={{clade
                                |1=''Hyaenotherium wongii''
                                |2=''Hyaenictitherium hyaenoides''
                                |3=''"Hyaenictitherium" pilgrimi''
                                |4=''"Hyaenictitherium" parvum''
                                |5=''"Hyaenictitherium" namaquensis''
                                |6=''"Hyaenictitherium" minimum''
                                 }}
                              }}
                          |style2=background-color:#ccccff;
                          |2={{clade
                             |1={{clade
                                |label1=''[[Lycyaena]]''
                                |1={{clade
                                   |grouplabel2=Cursorial hunting hyaenas 
                                   |grouplabelstyle2=font-size:12px;font-weight:bold;color:darkblue;text-align:right;
                                   |1=''Lycyaena chaeretis''
                                   |2=''Lycyaena dubia''
                                   |3={{clade
                                      |1=''Lycyaena macrostoma''
                                      |2=''Lycyaena crusafonti''
                                       }}
                                    }} 
                                 }}
                             |2={{clade
                                |1={{clade
                                   |label1=''[[Hyaenictis]]''
                                   |1={{clade
                                      |1=''Hyaenictis graeca''
                                      |2=''Hyaenictis almerai''
                                      |3=''Hyaenictis hendeyi''
                                       }}
                                    }}
                                 |2={{clade
                                    |1={{clade
                                       |label1=''[[Lycyaenops]]''
                                       |1={{clade
                                          |1=''Lycyaenops rhomboideae''
                                          |2=''Lycyaenops silberbergi''
                                           }}
                                        }}
                                    |2={{clade
                                       |label1=''[[Chasmaporthetes]]''
                                       |sublabel1=(running hyaenas)
                                       |1={{clade
                                          |1={{clade
                                             |1=''Chasmaporthetes exitelus''
                                             |2=''Chasmaporthetes bonisi''
                                              }}
                                          |2={{clade
                                             |1=''Chasmaporthetes borissiaki''
                                             |2={{clade
                                                |1={{clade
                                                   |1=''Chasmaporthetes lunensis''
                                                   |2=''Chasmaporthetes melei''
                                                    }}
                                                |2={{clade
                                                   |1={{clade
                                                      |1=''Chasmaporthetes ossifragus''
                                                      |2=''Chasmaporthetes'' sp. Florida
                                                       }}
                                                   |2={{clade
                                                      |1=''Chasmaporthetes nitidula''
                                                      |2=''Chasmaporthetes australis''
                                                       }} }} }} }} }}
                                       |style2=background-color:#ddccff
                                       |label2=[[Hyaeninae]]
                                       |sublabel2=(bone-cracking hyenas)
                                       |2={HYAENINAE}
                                         }} }} }} }} }} }} }} }} }} }} }} }} }} 

`,
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
