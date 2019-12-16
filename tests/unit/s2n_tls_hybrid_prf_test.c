/*
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <s2n.h>
#include <stdio.h>
#include <string.h>
#include <tls/s2n_cipher_suites.h>

#include "s2n_test.h"
#include "stuffer/s2n_stuffer.h"
#include "testlib/s2n_testlib.h"
#include "tls/s2n_prf.h"
#include "utils/s2n_safety.h"

/* Test vectors calculated from an independent implementation:
 * 0: premaster secret part 1 from the classical exchange (ECDHE)
 * 1: premaster secret part 2 from the KEM (BIKE or SIKE)
 * 2: client random
 * 3: server random
 * 4: client key exchange message part 1
 * 5: client key exchange message part 2
 * 6: expected master secret
 */
const char *test_vectors[][7] = {
    {
        /* ECDHE + SIKE 1 */
        "e45c242e720129feaafafea3dccb73b5562906657505525db4074c403215284992df25062a61091651dd5e9dd3401a72",
        "4346C330BBB2526CECFCC8238FA86913",
        "0102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20",
        "2122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F40",
        "16030301FD100001F66104aaaef1b8bf482280aeb8eaa9ef104b12f9526c58ba4d0223e2db988284251dd0755744ccb7e3addcd6757d4e"
        "2b9f9829275cd152f9c99b67df8cb5de032aa593a23b66d20b6a9c9670cb49d593b8d8fc954978545cd8d57e758625ec67dc8f80019289"
        "611D5DB90BAA5BCC1F076B0B1FA275D8EAB09FD4EBD8B5D05D0864F95815CEF9F7952612968A459FA5C5F3791534916EC4F77C44CC59ED"
        "0EF96E44D45020B381FFD4F974AF89D41017C95B04E852174307B629D8479737BF3B5A597FD7689B00D2078D0D4D45166C49ECE65FB84D"
        "00EFF1E0A70D5727306865EBC8FF25C6F718BD4EFCE230A9317A01ABB35DBD00004146B9",
        "C9EA33FF4C43F541E0AFC23A7409F769AA8B25FF0AA6A3E41A7C7ADBD02043DAE72B794F1EBF123DAE1E06782D9F1287EE5D88813BE64B"
        "FD0B67D751AA6AAA6FC3B27D3F7FD9766D9B9AA5EF3CD4061898F37D916CD9378931EBF0234F00932200F2489ABD35944328D091781049"
        "70E9BBE25FA81ACA265DACF045A81897246A347B6CCCF70CB65E375A6F629D847A48AF98DE8165C3AFA882B5143CF2F453B5A39A5329E0"
        "91542EE40B5D16367808F536EC39761B37D635943D312FF1DCDAF2254FA45D549DADBF5A999CBF1D9985908AA3D740DC59138EE19ABA88"
        "2B3D4758B72C0DB81D681AAE44096514DBFF5E9512687025808CA10F45D395DF515FB0",
        "70875ee292ce7867f3c07399566cbc7933ae39dcb395f4c72c27f26cc0535858a2524ca5842aa1ce2fa5bb53b5d9415b",
    },
    {/* ECDHE + SIKE 2 */
     "e45c242e720129feaafafea3dccb73b5562906657505525db4074c403215284992df25062a61091651dd5e9dd3401a72",
     "B5DB5992AD277FE747BEFB677040FFC0",
     "0102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20",
     "2122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F40",
     "16030301FD100001F66104aaaef1b8bf482280aeb8eaa9ef104b12f9526c58ba4d0223e2db988284251dd0755744ccb7e3addcd6757d4e2b"
     "9f9829275cd152f9c99b67df8cb5de032aa593a23b66d20b6a9c9670cb49d593b8d8fc954978545cd8d57e758625ec67dc8f8001920BA524"
     "F9F01CAA22CBFBFE744AEA8D2C0CD534291424B613077422D25435024577ACDF49356B43BA0475428354E3E294070D57AEE29DE813806004"
     "56B39F0E0DC959372C94B883A5D20C5EB879B54D825CEA693AFCADB8DEEED43A51B5FFF177D81D50B13F5357837FA8B349177E0B14FEC790"
     "989E2F6820C9054914F51C9B4BC773A527246FA13778E28A41A9FABDA71C3C2A",
     "9C0BFCA7393A4F271F531CCFD3A40F86273094D1EACA7541C037D6EE62D7D370D4670731F845EDA1E920263EA9E0F574566048270DCB9AB0"
     "7D7B3E565619AC2BEA499CDF52771C09AF3B61B02781D13A28966DCB117733C31A4CA361A9AD5526F892C47F03BDEE462103DD14B8CB1823"
     "2F174C028AED04306650299F66A929ADBB1EE7FC22D03FD2BAF88911EFF4B74C3762724E9E5A33DBE27EAEA743540EB75E9C7625FA519C12"
     "B67C1E88254E4796801104ADF702B4D2BB9D2BE7D268759630CEE00FD50A1A0B43696B1F96708028C06AFC2C3B4FBEBA40B4E9496B4A9D33"
     "3A6A100362CE0EDB5C07F12FE29A22D1A86012DE9DE39F394C5ABE32444E7B",
     "4f267805d20f00c91070a34dbfe81dd865a86c03f7ec7fe6df16da2bcba7ad4e62718ec81c24ae13165036bdd44c5c55"},
    {/* ECDHE + BIKE 1 */
     "e45c242e720129feaafafea3dccb73b5562906657505525db4074c403215284992df25062a61091651dd5e9dd3401a72",
     "E7D65CF208E78FB6F747E1F0A00E23B136C51EDD30EE5B1B210A4459766509EA",
     "0102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20",
     "2122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F40",
     "1603030A5610000A526104aaaef1b8bf482280aeb8eaa9ef104b12f9526c58ba4d0223e2db988284251dd0755744ccb7e3addcd6757d4e2b"
     "9f9829275cd152f9c99b67df8cb5de032aa593a23b66d20b6a9c9670cb49d593b8d8fc954978545cd8d57e758625ec67dc8f8009EE93F536"
     "6397957BBCBAE45C09818915F092DC0FEFFB7421915D84B376F1EED36B1718A7E2D0B2E4DA937E29B0B693BE248F567761D73BDA342841AF"
     "E55AB94FD7C210EBE125E29B2C8EC2D5AD844B05AFF5F141F5007DBBBE41E04D913234B0D99A370FF9B1938102C93D1E36F30636A33140DF"
     "06D2D8CC60ED89C446DCC76221C021045156D50A1836C530A2FCE767D62A650871731DF4D0EA0FCB175A09CA9CD0A44DFC4F4143181862D6"
     "146F5EB43C7E70C6C3E6626C6271506C27F2B56A94F4633571D5998A3189C6215E1BFA98C931DBEBE47B2589A13BC1C10153434202A235CF"
     "94FC8D256580CAEB200C7D99E0DFE4B65E5CFB8FCF5B0B13E59039A1CE7FB3C916D0B41E6CAF6568E85DA7F4AD0A75F6848CDD2CF7912C05"
     "7731E359AA4075D2DAFEE5F6B68B76C22DD994C89B35F53E5EC17F2623A30F79AFEEA5B3D6CF378D7A1C358F5AD1E9BC26FCD3BEDCE6F6AF"
     "992D60C88379AFA5D38DE5A565983522E936B382F138268425C2F07FE5064CD0990FDD372D8DECD1EB4972CABEA90E019DED68FE0EFDFFCD"
     "C4BD763654B9ADC72E3F776C7AF7723D3FFC3B70AC5A6C3E53BD69C8B31881DE22157984646F5F191F73A30324F687C1724FDBBA149B5CB1"
     "2F1FD6D626B13FE6781501B9A209BF7226B8EA6DC7B43784DE079AA7849925D6F09DD1F0ABF3537B2DC05304D5E65BDE22A75EB8CF9DD21F"
     "5A44DCCF3F02AB14BE42E35E110CE6DF2EF2D68D30A87DB3BA8235E70B458F41F65ABDBC463D46C007624FB8E7DE0C8CDB6E12713421AA52"
     "B990E1811AAFA9B8922FB2D85195B9EFD0DC5F6A3F0677B970698CFB3C0E89756AA8FABA7647DB1EFAEE69516E0D06159BA4068DB255287C"
     "3253AC4B9E96CCE1ACA185F325E48410B15A83104334E5C00DC1305B6B39B602F5077DDE10EB95E6F3BAB293748D828B2C1953419E8C77FF"
     "6087659A1ED74633E39E9938A4C5C7649B7FD7F04E1DD3F30DEE24E20653EE0C8D9B64F1E3A368B3B425CAD322751C0ADD601618AFBB7EEB"
     "2F40F496E4803A895F747A33E2C2980932812B1B012B5178EB928E79FA21AC04E12AC9F36F619CDB45FC6DECAB669FA3B6145F3E0CEEDC79"
     "21B824A89D84741C1F020F5E2E1AA4A091D30B79B2C067F2FFF98172D4B756EAAB3CB63B1067297D7744D188B19E4874597B14BD13A61508"
     "E3E33B4E1797535C52651C6F2BA9A8AF536E8A3A4675EC52C616E035EE37C355553E19C8CDDEBA74C125C97F63898EA5833AA5B8D353CE00"
     "0A9B7C916FA2B0D39297E898AD440C0A9F0FC1F7BB3F0E8000D29A14CA5EBAD1DF05B60A12AB83C19FC2ADA8396CF3BAF8E334F7A67C160B"
     "8BC471C2F7CE8AC1FD7ABDDBBD7E7E46EF605F094F4AAFDCAFD51789F191976CB7DC0CF95C66D66274D758CE8146AE04891396B1B6051DEE"
     "10A6654BAD45091FE305B93122EB7A2DA254D388E46120E271FA3B25C4DD977F0336459F4E77BB49B5D80F2208E9BA6A62D4F6EC4E3E279F"
     "4B8198DC6C2257BDE66242E63E7D782535543AF2438F5A4B681499A943DB38B4CF84C641294819A72E3E1D5E1B3EB1E204F8EFC4C46FFB4B"
     "BFEB4D153620A824038B6DC59E4CA5CD0B1DEF135CD10B69FBE745A33ADE3D4EBA84F4299DC7E9EAB789018265A1C5D6A806187A8FF3CEE8"
     "EFECB269C8828D7455F0F257E8307545F7C1A1E090EC5786AC4FB7B8B79EDF449F43E4C867F8",
     "D755305FB97296A780CAFA8CC963A5AFD7BF126AC81CDAF1FF0F81DF94372A8148651EFF6EC87F3A3FF5CB8DB2F5BE1047717F05AB01DA9A"
     "8FCE9C676031328BB57CFCD7781F2E25CD9605F88F881C4C99D8B5D061503CDB9001F5F5B334C553FC3397C68F58F72C8CD9BF53A6B74D13"
     "FFCF7B2A6A3518407AF28E8E5755B97066E9CE73C56DA2202F22B6768880E7FF50D20E13A9FF07D1932C989FDEA6226C4334B8DFDC781EEE"
     "058C19B0333B1A2E9993B54ECA233846EFBAD11A934016B1FD443DEF83D8E2E5003EF8DA15EE1D787BC0F6CB0A17717A1A4045B3624E2F9E"
     "E713590D1216F9372E801DF9AF1A27690442398E96B6FADBFCC535918F7C93CE190A950446183B04BEEDD4BFF8C121E4C1850E10061F0726"
     "BE49CA6C4D116EB99FDAC7F9AB54493F0E7D7496EBD9A6ECC7D3719F30A370242556EAF9FAB8798AA0A5C1EE4154F60246DFB643D9B8B2C7"
     "01D4B234A4E4B36FA34FF84F4BA86ACBB1A17A32551B98F1FD68F372E91CEACFA006B755697186B91F322EF15D42E9CC7FF7A3CA60A88B91"
     "14BD5F0710A10B6BA5BCE032AEE487F43679A5D68345B561ECE82CA7E2628A57979186E5C45C626F42E944CD56C43EF47713100FCFF78983"
     "B72E75C4FD6353C7F751AFAD9F15FFC6AA5CFE1FFD4FBE181D8ED92908C7201A8F8322FA43DCDE9AA9A7170A00F2AD38CF6D701A5346207F"
     "DADC6AF29FF2F8C288C310CE09D39861EE3B39464457A4B57BB43559CD4CB5BF78190F8B9D4104F997E3B2612DCAB0E3F2A7913730DD32E6"
     "F23344D694A2AAB0EE183EC3094AA7BCC163A5DCAA7D895EFA4AE508F1FDD7B4EDC611F93254F1AACC31AD32CFADF89E5BBC03B594746363"
     "B1B965B4E656F2CEA0A5A9453AA1432190DEAA6A0D4EFD4447A305DD38A276747FCD6BE4163E24755E19612780A547BCD3542409292554DC"
     "BEA480AFBCB3D14AF4B47102D597F8A9A3CDEE11AF08F629D775EFE4D9C3C759E192E8AEDF4FF4166B32A664B406CE07EA212AF2E6DA8FB3"
     "3DD475AF536E0B85F8A25C1B671ABF2B9D4C704FF6A67005064E722A163A8EFC840FDE0EC7778F2AA6F30439A6CF9BDA5DBE2CB14C29BF1C"
     "3D8D708A1BDE12387860296677BCD0318749ECD3CFF666DD58646435B0A736BBC3DACE5E03FEEBFB49B2842A7D0BCA2C3A1E590364FD1AE7"
     "3DCDD9FF0D85A08236E044781448973B8F46294CB0A04407A6E12473A317811182ED1D519D8212FD20EF68D027B58027ED58D1B0F82CC83D"
     "3E5D832024B688C5AACB6372C74D31A05C08BA9F34431CCC6FC34B4C46F2FC8B8DEEF28ACD478791991639F89BD2F6E6B868D0A3260AEE16"
     "86EB7D1DF05F4CAB8A0D15D76FDF55C3C74A653D4F0D6126BC49CAFF6763013A06D6EAC3AA20553C6F2B01138CC50379F3B00446B4B74311"
     "13A60EBF5E3525B45E2582E241E5B07FB9056D22ACB7F570422B2A54E0AAFF4AE6E55C33948EBD703376B257EEB07D2149E7B9185E06A9A9"
     "668771B91F089C24B151F9E5055A72F04F5B1D514BFBCECC1A1939E8878208BA247E7A85890458A2E8988A7DC34A9A5337222EEDAFBBD7F7"
     "218696F67C741C645FA87C0362AEE545252E52FA2EB90DB5A44C6A196288F48BACBEE4A3BEE3D2005971F96F7D7E4BBD370133D12CA717F8"
     "7998F5E87C0EE8C1747B2B2038C0E8879537CAFFA98CE18A869844E0915A19A9FCCBABDF70FC04D7055AEC2F8ECC7CB956263E2D53AB2B42"
     "F7FFE0ACE78B42E7FCFD41687A0C60C9442C9BD493627853B52B62783646495DB51C06869790E2729870FBF09BADC04FA4AF627B09E3D6F9"
     "D335BAE913585E3E3994AC12098A5F0A7B7A2FCC37B6B8B453E8AA09A17A34DDDDB455CE05",
     "32f90bc7b54adffa37c882d2b8a954ef417b892eed033739a2f12f928c16aea99f349503cbd7e9a9073b7a37512f7e52"},
    {/* ECDHE + BIKE 2 */
     "e45c242e720129feaafafea3dccb73b5562906657505525db4074c403215284992df25062a61091651dd5e9dd3401a72",
     "0F6C9EC4D79B2201E500121A18632443E0F4DFB4236D12DA911C2FB6095DA333",
     "0102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20",
     "2122232425262728292A2B2C2D2E2F303132333435363738393A3B3C3D3E3F40",
     "1603030A5610000A526104aaaef1b8bf482280aeb8eaa9ef104b12f9526c58ba4d0223e2db988284251dd0755744ccb7e3addcd6757d4e2b"
     "9f9829275cd152f9c99b67df8cb5de032aa593a23b66d20b6a9c9670cb49d593b8d8fc954978545cd8d57e758625ec67dc8f8009EE4C1732"
     "301C371700E88AC3D16CA1F156CE1C328D9BFFAA08768DD6662C30D6BAD7801D9CF4013ED5CBA2E11047BA262DEF4116B49E8EEFDD96CFCD"
     "2378EE517D57C64EC14038ACD2E5C1EDFA3570BD67801C20D03991F1551632913CF6DAE357991FA0AAEF12A845CC4BDDB276A438FE71E0E1"
     "A0D9C602790CE6CC80248E9F4BA974B0606AB27E07D2DC64878F5B1A499956C32D403D14D63DAE116A7A135FB21CD2DF973129A23C71BF44"
     "73D12FEE71391C9D78AD0F06712E69693DEBB73406A78475D55CAA7A8A8852858E0AA99649B43EF89CFE3AE5BA893AD1A5FCE0AEFA14760A"
     "0A1445C31F495CF41A8CE73FA18815C25E7372FD4D4D60A54BE2808BFD8F1724EAA67AFFE3B84E942F0C0776EDCE8301F1FE1604713F8179"
     "18DC4A64708D36DAF6388D29FF7F6575C619480709E77C16EEBC2B10DABD823EA242AFF8EB5B4715E523AB0FC1B1CAA12C855B79B3E67195"
     "8D4707DE6E6EC7E86C56B4B82F1B5120974EB08822E462A080487D9369799F12FB2F78C026C4D35FFC4624C471436347599231F820D4CFC8"
     "2A7D8377C38B9C83C4B890159D5C10FA2F045A1BF3C49AAABB994B1471D7AAC31475EEA92A6AAF4619A4D59AF7120A8FD93938147FEB3587"
     "AEFDB25A95B0152A0E48A9EA17AD24DB02595B87E2159FB5145E089372914FFD5C7C5ACB5225B501D3F09E39511F09EB87850CD9A77266CA"
     "E1E093941F991C931391C2B40162958CE89DFDE4DC6AA63F21CD2ADDA8211B3EDFC3986DE01AD597586576CF3FF9E89E9D48CF74310494BF"
     "02197F1888835095A7AC07CFA0E7C97D4C4052B0E0DE5258CE0F40622CAC18756D0F066945572D27826CDC8970DAEF60C9854AEF735CDAAD"
     "9AE2F341F0A543C44990420FABD8B5D2A1973EA681A1E3C515D48708F24E84C07FC2EC2E0210140ECC1FBF4BFF950590424CD790381737FF"
     "4911B75BD9A232DDACBFBEB3D4B31FD0C3F0B91950D2FC891B392ABE737B8B9CFEDC037B973A3A178B48AD87FE3F69D2BDC4B51CE82F960A"
     "056C4C4642FA2294C64304732178362A8229124B738277514B98FA92DAFA7653C73B0592BF9457E10DBFC48D2DE27F9C0B4B3B4C35E24A42"
     "35E94F502CAF3695BBF9DA9E2C4BB8C73884A6957BE3F6F67959A64F732C8D6F2D764C87A471CC10A1A8E02EA125CF9772B854084E168F56"
     "7920BA84B3981F91B56BB568EA74A0D8D0D2714B11B1B7E3BC8FED5CC4A17039CFAA813B72E879661E00034DA4863F23D923DB9A2F6346B5"
     "A3DE8B8A31D475E8092F069467369C9A527777A1F4025C930073726CB7D934E6376A5AA629DF05854D7976809C95E7897B43D305797718B0"
     "A135A0B105DA7C979049552B2079A61B03D01A9185FC6ACB010B37C9A23C491A43B234E9D6905E94F0EFEE01E4A24783A1DFEE8F5A4E9072"
     "1508D802859E0BA210EB59DD09538EEDBE7C997EF0E1E67BFE6EA6696F0B655498A57F4E010A70187295B052B37DFD59BC85CE4ADF44C5F5"
     "432F27458E97FA0E69DAA21C5966B74393DA9605D7665FB0448BC378D6DF02A22B02CDC70D400DD7C715620EAB7F398375391349115EB213"
     "895803A9786A98231AB159597CB841407BD8E27613EE3C8240B70955D2ED3D8A7ECF36786193D5193AB9B9367C4F7B16AC08B30BA1294207"
     "9C2972BF6B741DA57A0D8D9ED595FE2C21141C1FF0C4D3C74C95216C78BC5A740AE1C489764C",
     "FB7925E49097FA4717A5FEA040F2A64D9E7013CCC75D89834C83E523D1AF28E0B4FD72CD04A52FE1012E26037E40ADB1BD3FE22E99049316"
     "9C3B040651A012D458078F3B3BB03CB7C0E266B05C85E6D0CE53A3675D08B262D5B5BA670504A61829E03BA5B04E7F59C0DEC5E50343E2AF"
     "FF7B98279204FE8CD7B490A0E100C7C1DF5EADE1238263F1C016D89C32C2D56CB6341CEAEAB8347EF3D9E02EB649B2F59A7528063AB0F09D"
     "29CAA91542ACF9D22664C8ECA8DD331275DD01558029B9D99B777ECCE6A796F2174DEECECF64DEDCF2E441F4C228F43F3676BAA9592284DC"
     "F13A214D59505234F4ED8A9D155A74B6E7E4E51DD782FC868AF7A030F2F52ED37CE4D805E936C5A48BCD2896E897AB6D93F5DE2BC1F5BB8B"
     "5A3062AE578F78ADC64DF429D2024CBFFB93BEA6BE9879DBA597F19166B6FF9F96D8E5ECCD8934AFB70562FE38F8445CE3917FF5D3E10B15"
     "78245300C3041E54B1C87EB3BB3A9C341E0FAB64F9001D5FA89B50DD48D5DEBCA542D7168693D1D4BF79B137EC257AA104D8892D7C998D4F"
     "961D0EE4E3DB36C794610F99486B2C00E3144043D0BABF2FA60805831B9BE7240C77A7F1F41EA639303C8012D3CE1969BA1432F1E60BD1B7"
     "CC1F34BE6A2EADECDC0FA6FE39616DDBCB5B942F8252427890B1DA8AEC422EA5EC6CD29701B29218536B603CBBD24E4F3F32C312783073DD"
     "1C4EC620EE71B287CFA872C1ACEB71D497E104D4D9244F08EBF71D198A21ED5DFFFDACED5DA5792599DCB19EBDED3E76F102B9AF09BD0C35"
     "0BA7048507071CA5649EC03B02BC12E7CFFBB03EB5164BD0958D9F52EDDEC2CC8797683CED6C583A254F66B7B3B62364779ACA13B3855BE2"
     "AFF1A235F10320F29D42D99514958BF698904BEDC75392E762CFD45DF74E74B84FEB63FB49D8465F0B71B25A2830EDBBC0BF8D3C13E0554F"
     "5ED6AA321F2A4BE72742B0AD19ABB71F50FEF362A35E5FCE8F25D288D00839A730A4E176D074F8FE1AFF0A3F2AE38F60670C12006F37345E"
     "A207E44C13FD012906A8F1FCFBCC4D87834B7051807D1DA6D955EA8A20CE52DB552A39F1CB45069CDD51D1BDA73111E68A6D2EBA2A747118"
     "E85CBC280C53F7E452211DF82AB258AEAABFF2CF6EFB823991D9D8F98A117CA6488489BEA87AC126E9DA3C2E3A2735D5FEF6102D642E8A26"
     "E611C8C5045F84E207DE85FAB89ECBEFDDCDE9283C1A5AE5B7AEE176F7DDA0D9433C5E9D5E18264F6673C9AB1A04DA51F11294D1225D592B"
     "F3934F1CF3EC621BE08B04F41DD9D3F1470328CBC0228CDDF0941DDDF0BDC61E9586372E5D30F5BEF2DEB3A0B669606E82B4CC82A7AD495D"
     "2E023263DDB876D1B98A270EA5107DBFD8A4D30A7732A5DE58997B682200828E4FC92F19D8DE51AC227E44DE81B97267C485B59F2A9F5C30"
     "64CAE6C011529D19FA28BDCE7E6216148B37DFB46E3D84D64A3338EF5C284A037B0C1BCCF6DBBA7857789D078D27BE1728CE6C35C33B984F"
     "C68E542C16285988C20AA75052342494CD18BA0A3599D04170489B5F2DD67E061C5F084C327E9D3A0DDF8CE9E52E3FFB07DCF4A08DAA8861"
     "A536E8F46B91C69FFB6CA1EAC3B1CFB263019ABF0C7952352A357199006F63C162D4E519DB1480101119990F9D3CAAAAC2753BE8D0A25636"
     "777CE95B14A8446879E395AF19F01D3368768FB8696D9EE60C49EB8B6B2638936689611364321203AB9873016EF88BE0E2D1193C7C38F432"
     "DA22D3293AA34C6669553709DA018F7D033D819A2E45F7450FC60F3BB8B33C9FEFF0965F637D71113587DDC9467CC90E182CB2B1A8B508F7"
     "9E2598F5F9A3C2B083A44598057A37741023477DC044C97ECB2F886D3A6344229815FEC007",
     "91b9f0971edfa48b0dacb2dc49e2da3555b3178e65a6e2c233fb1e869dfa98ee7c33e90226e22e31b897cc923994198b"}};

int multi_convert_hex_to_bytes(uint8_t *output, const char **input, int num_strings)
{
    int output_index = 0;
    for (int i = 0; i < num_strings; i++) {
        DEFER_CLEANUP(struct s2n_stuffer stuffer_in = {0}, s2n_stuffer_free);

        GUARD(s2n_stuffer_alloc_ro_from_string(&stuffer_in, input[i]));
        int num_bytes = strlen(input[i]) / 2;
        for (int j = 0; j < num_bytes; j++) {
            uint8_t c;
            GUARD(s2n_stuffer_read_uint8_hex(&stuffer_in, &c));
            output[output_index] = c;
            output_index++;
        }
    }
    return 0;
}

int convert_hex_to_bytes(uint8_t output[], const char input[])
{
    const char *strings[] = {input};
    GUARD(multi_convert_hex_to_bytes(output, strings, 1));
    return 0;
}

int main(int argc, char **argv)
{
    BEGIN_TEST();

    for (int i = 0; i < 4; i++) {
        struct s2n_connection *conn;
        EXPECT_NOT_NULL(conn = s2n_connection_new(S2N_SERVER));
        conn->actual_protocol_version = S2N_TLS12;
        /* Really only need for the hash function in the PRF */
        conn->secure.cipher_suite = &s2n_ecdhe_rsa_with_aes_256_gcm_sha384;

        const char *premaster_classic_secret_hex_in = test_vectors[i][0];
        const char *premaster_kem_secret_hex_in     = test_vectors[i][1];
        const char *client_random_hex_in            = test_vectors[i][2];
        const char *server_random_hex_in            = test_vectors[i][3];
        const char *client_key_exchange_message_1   = test_vectors[i][4];
        const char *client_key_exchange_message_2   = test_vectors[i][5];
        const char *expected_master_secret_hex_in   = test_vectors[i][6];

        DEFER_CLEANUP(struct s2n_blob classic_pms = {0}, s2n_free);
        EXPECT_SUCCESS(s2n_alloc(&classic_pms, strlen(premaster_classic_secret_hex_in) / 2));
        EXPECT_SUCCESS(convert_hex_to_bytes(classic_pms.data, premaster_classic_secret_hex_in));

        DEFER_CLEANUP(struct s2n_blob kem_pms = {0}, s2n_free);
        EXPECT_SUCCESS(s2n_alloc(&kem_pms, strlen(premaster_kem_secret_hex_in) / 2));
        EXPECT_SUCCESS(convert_hex_to_bytes(kem_pms.data, premaster_kem_secret_hex_in));

        /* In the future the hybrid_kex client_key_send (client side) and client_key_receive (server side) will
         * concatenate the two parts */
        DEFER_CLEANUP(struct s2n_blob combined_pms = {0}, s2n_free);
        EXPECT_SUCCESS(s2n_alloc(&combined_pms, classic_pms.size + kem_pms.size));
        struct s2n_stuffer combined_stuffer = {0};
        s2n_stuffer_init(&combined_stuffer, &combined_pms);
        s2n_stuffer_write(&combined_stuffer, &classic_pms);
        s2n_stuffer_write(&combined_stuffer, &kem_pms);

        EXPECT_SUCCESS(convert_hex_to_bytes(conn->secure.client_random, client_random_hex_in));
        EXPECT_SUCCESS(convert_hex_to_bytes(conn->secure.server_random, server_random_hex_in));

        EXPECT_SUCCESS(s2n_alloc(
            &conn->secure.client_key_exchange_message,
            (strlen(client_key_exchange_message_1) + strlen(client_key_exchange_message_2)) / 2));
        const char *strings[] = {client_key_exchange_message_1, client_key_exchange_message_2};
        EXPECT_SUCCESS(multi_convert_hex_to_bytes(conn->secure.client_key_exchange_message.data, strings, 2));

        uint8_t expected_master_secret[S2N_TLS_SECRET_LEN];
        EXPECT_SUCCESS(convert_hex_to_bytes(expected_master_secret, expected_master_secret_hex_in));

        EXPECT_SUCCESS(s2n_hybrid_prf_master_secret(conn, &combined_pms));

        EXPECT_BYTEARRAY_EQUAL(expected_master_secret, conn->secure.master_secret, S2N_TLS_SECRET_LEN);
        EXPECT_SUCCESS(s2n_free(&conn->secure.client_key_exchange_message));
        EXPECT_SUCCESS(s2n_connection_free(conn));
    }

    END_TEST();
}
