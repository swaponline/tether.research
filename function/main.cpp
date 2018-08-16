#include <bitcoin/bitcoin.hpp>
#include <bitcoin/client.hpp>
#include <string.h>
#include <iostream>
#include <boost/asio.hpp>

using namespace bc;
using namespace bc::wallet;
using namespace bc::machine;
using namespace bc::chain;

#define CLOSING_CHANNEL_FEES (4000)
#define MIN_AMOUNT (5600)
#define DELAY (600)

#define SATOSHI_FOR_OMNI_OUTPUT (546)

data_chunk uint32_to_data_chunk(const uint32_t& value)
{
    data_chunk returnerValue; //32 бита
    for(int i=3; i>=0; i--)
    {
       returnerValue.push_back( value>>(8*i));
    }

    return returnerValue;

}

data_chunk uint32_to_data_chunk_inverse(const uint32_t& value)
{
    data_chunk returnerValue; //32 бита
    for(int i=0; i<=3; i++)
    {
       returnerValue.push_back( value>>(8*i));
    }

    return returnerValue;

}

bool validate_tx(const transaction& tx)

{
    client::connection_type connection = {};
    connection.retries = 3;
    connection.timeout_seconds = 8;
    connection.server = config::endpoint("tcp://testnet1.libbitcoin.net:19091");
    client::obelisk_client client(connection);

    bool val1;
    static const auto on_done = [&val1](const std::error_code& ec) {
        std::cout<<" Transaction validate:"<<ec.message()<<std::endl;

        if(ec) val1=false;
        else val1=true;

    };

    static const auto on_error = [](const code& ec) {

        std::cout << "Error Code: " << ec.message() << std::endl;

    };

    if(!client.connect(connection))
    {
        std::cout << "Fail" << std::endl;
    } else {
        std::cout << "Connection Succeeded" << std::endl;
    }

    client.transaction_pool_validate2(on_error,on_done,tx);
    client.wait();


    //return allPoints;
    return val1;


}

bool broadcast_tx(const transaction& tx)
{
    client::connection_type connection = {};
    connection.retries = 3;
    connection.timeout_seconds = 8;
    connection.server = config::endpoint("tcp://testnet1.libbitcoin.net:19091");
    client::obelisk_client client(connection);

    bool val1;
    static const auto on_done = [&val1](const std::error_code& ec) {
        std::cout<<" Transaction validate:"<<ec.message()<<std::endl;

        if(ec) val1=false;
        else val1=true;

    };

    static const auto on_error = [](const code& ec) {

        std::cout << "Error Code: " << ec.message() << std::endl;

    };

    if(!client.connect(connection))
    {
        std::cout << "Fail" << std::endl;
    } else {
        std::cout << "Connection Succeeded" << std::endl;
    }

    client.transaction_pool_broadcast(on_error, on_done, tx);

    client.wait();


    //return allPoints;
    return val1;


}


transaction createAtomicSwapTetherFundingTransaction(const ec_private& Alice_private, const data_chunk& swap_secret_hash,
                                                     const ec_compressed& Bob_pubkey,
                                                     const uint32_t& locktime)
{
    payment_address Alice_address=Alice_private.to_payment_address();

    transaction funding_tx;

    funding_tx.set_version(1u);


    //входы транзакции(в идеале лучше было бы, еслиб программа сама находила и строила входы, как например с помощью функции getUTXO, суть ее в том, чтобы обраться к серверу(в данном случае libbitcoin серверу) и отправить запрос на получение данных из блокчейна, из которые уже можно бы было получить UTXO для конкретного адреса,но сервер возвращает пустое множество UTXO, для любого адреса. долгое время не смогя разобраться с этой проблемой, было решено пока возложить на пользователя задачу найти и вписать свой UTXO)
    //выходы можно самостоятельно посмотреть на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //тут необходимо ввести хэш транзакции в которой есть непотраченный выход
    std::string PrevTxHash_str;
    std::cout<<"\n write your prev UTXO hash in base16, once output from this will input for channel's creation's transaction:\n";
    std::cin>>PrevTxHash_str;
    hash_digest PrevTxHash;
    decode_hash(PrevTxHash, PrevTxHash_str);

    //тут нужно ввести индекс непотраченного выхода, индексация начинается с нуля, выходы например на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //расположены сверху внизу в порядке увеличения их индекса
    uint32_t PrevTxIndex;
    std::cout<<"\n write index unspended output of your UTXO, its output will be input:\n";
    std::cin>>PrevTxIndex;

    output_point UTXO(PrevTxHash, PrevTxIndex);

    //сдача пользователю, то есть открывающая транзакция (opening_tx) имеет 2 выхода - первый, на счет с мультподписью размер отправляеных на него биткоинов равен ширине канала, второй - сдача пользователю, остаток средств которые он хочет вернуть на свой адрес, он наберет их сам, учитывая какую сумму он хочет потратить на fees
    std::string OddMoney_btc;
    uint64_t OddMoney_satoshi;
    std::cout<<"\n write odd money (in BTC), for creating p2pkh output on your address. Dont forgot about transaction's fees:\n";
    std::cin>>OddMoney_btc;
    decode_base10(OddMoney_satoshi,OddMoney_btc, btc_decimal_places);


    input input0;
    input0.set_previous_output(UTXO);
    input0.set_sequence(0xffffffff);

    funding_tx.inputs().push_back(input0); //добавим вход без подписи

    //создадим выходы транзакции (2 выхода:
    //первый - 1 на адрес с атомик свопом
    //второй - сдача самому себе, выход со скриптом вида p2pkh

    operation::list SwapScript;

/*    SwapScript.push_back( operation(opcode::ripemd160) );
    SwapScript.push_back( operation(swap_secret_hash) );
    SwapScript.push_back( operation(opcode::equalverify) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey)) );
    SwapScript.push_back( operation(opcode::equal) );

    SwapScript.push_back( operation(opcode::if_) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey)) );
    SwapScript.push_back( operation(opcode::checksig) );

    SwapScript.push_back( operation(opcode::else_) );
    SwapScript.push_back( operation(uint32_to_data_chunk_inverse(locktime)) );
    SwapScript.push_back( operation(opcode::checklocktimeverify) );
    SwapScript.push_back( operation(opcode::drop) );
    SwapScript.push_back( operation(to_chunk(Alice_private.to_public().point() )) );
    SwapScript.push_back( operation(opcode::checksig) );

    SwapScript.push_back( operation(opcode::endif) );
*/

    SwapScript.push_back( operation(opcode::ripemd160) );
    SwapScript.push_back( operation(swap_secret_hash) );
    SwapScript.push_back( operation(opcode::equal) );

    SwapScript.push_back( operation(opcode::if_) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey)) );

    SwapScript.push_back( operation(opcode::else_) );
    SwapScript.push_back( operation(uint32_to_data_chunk_inverse(locktime)) );
    SwapScript.push_back( operation(opcode::checklocktimeverify) );
    SwapScript.push_back( operation(opcode::drop) );
    SwapScript.push_back( operation(to_chunk(Alice_private.to_public().point() )) );

    SwapScript.push_back( operation(opcode::endif) );
    SwapScript.push_back( operation(opcode::checksig) );

    script redeem_script(SwapScript);

    //составим p2sh выход, с хэшем скрипта redeemScript
    //OP_HASH160 <redeemScriptHash> OP_EQUAL
    //std::cout<< encode_base16(redeem_script.to_data(false) )<<std::endl;
    short_hash redeemScriptHash=bitcoin_short_hash(redeem_script.to_data(false));
    script p2sh_swapScript = script::to_pay_script_hash_pattern(redeemScriptHash);

    output Output0(MIN_AMOUNT,p2sh_swapScript); //вызываем конструктор класса output, в который передаем 1 - количество коинов, 2 - скрипт

    operation::list p2pkhScript=script::to_pay_key_hash_pattern(Alice_address.hash()); //скрипт для возвращает сдачу себе
    output Output1(OddMoney_satoshi, p2pkhScript); //создаем второй выход

    funding_tx.outputs().push_back(Output0);
    funding_tx.outputs().push_back(Output1);

    //после добавления всех входов и выходов подпишем транзакцию

    endorsement Sig0;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
    script::create_endorsement(Sig0, Alice_private.secret(), p2pkhScript, funding_tx, 0, sighash_algorithm::all);

    operation::list  sig_script0;
    sig_script0.push_back(operation(Sig0));
    sig_script0.push_back(operation(to_chunk(Alice_private.to_public().point() )));
    script InputScript0(sig_script0);
    funding_tx.inputs()[0].set_script(InputScript0);

    return funding_tx;

}

transaction createAtomicSwapTetherRedeemTransaction(const ec_private& Alice_private,
                                                     const ec_public& Bob_pubkey, const ec_private& Bob_private, const uint64_t& tether_amount,
                                                     const uint32_t& locktime, const data_chunk& swap_secret,
                                                    const transaction& Funding_tx)
{

    payment_address Alice_address=Alice_private.to_payment_address();
    data_chunk swap_secret_hash=ripemd160_hash_chunk(swap_secret);


    transaction redeem_tx;

    redeem_tx.set_version(1u);

    //входы транзакции(в идеале лучше было бы, еслиб программа сама находила и строила входы, как например с помощью функции getUTXO, суть ее в том, чтобы обраться к серверу(в данном случае libbitcoin серверу) и отправить запрос на получение данных из блокчейна, из которые уже можно бы было получить UTXO для конкретного адреса,но сервер возвращает пустое множество UTXO, для любого адреса. долгое время не смогя разобраться с этой проблемой, было решено пока возложить на пользователя задачу найти и вписать свой UTXO)
    //выходы можно самостоятельно посмотреть на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //тут необходимо ввести хэш транзакции в которой есть непотраченный выход
    std::string PrevTxHash_str;
    std::cout<<"\n write your prev UTXO hash in base16, once output from this will input for channel's creation's transaction:\n";
    std::cin>>PrevTxHash_str;
    hash_digest PrevTxHash;
    decode_hash(PrevTxHash, PrevTxHash_str);

    //тут нужно ввести индекс непотраченного выхода, индексация начинается с нуля, выходы например на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //расположены сверху внизу в порядке увеличения их индекса
    uint32_t PrevTxIndex;
    std::cout<<"\n write index unspended output of your UTXO, its output will be input:\n";
    std::cin>>PrevTxIndex;


    //сдача пользователю, то есть открывающая транзакция (opening_tx) имеет 2 выхода - первый, на счет с мультподписью размер отправляеных на него биткоинов равен ширине канала, второй - сдача пользователю, остаток средств которые он хочет вернуть на свой адрес, он наберет их сам, учитывая какую сумму он хочет потратить на fees
    std::string OddMoney_btc;
    uint64_t OddMoney_satoshi;
    std::cout<<"\n write odd money (in BTC), for creating p2pkh output on your address. Dont forgot about transaction's fees:\n";
    std::cin>>OddMoney_btc;
    decode_base10(OddMoney_satoshi,OddMoney_btc, btc_decimal_places);

    output_point UTXO(PrevTxHash, PrevTxIndex);
    input input0;
    input0.set_previous_output(UTXO);
    input0.set_sequence(0xffffffff);

    input input1;
    output_point FundingTxOutput(Funding_tx.hash(), 0u);
    input1.set_previous_output(FundingTxOutput);
    input1.set_sequence(0x00000000);

    redeem_tx.inputs().push_back(input0); //добавим вход без подписи
    redeem_tx.inputs().push_back(input1); //добавим вход без подписи


    //выходы


    //выход с op_return скриптом
    operation::list OmniScript;
    OmniScript.push_back(operation(opcode::return_));
    data_chunk omni_payload;
    omni_payload.push_back(0x6f); //o
    omni_payload.push_back(0x6d); //m
    omni_payload.push_back(0x6e); //n
    omni_payload.push_back(0x69); //i

    //version
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0

    //type =0 ("simple send")
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0

    //token identifier =31 (TetherUS)
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x00); //0
    omni_payload.push_back(0x1f); //1

    //amount
    for(int i=7; i>=0; i--)
    {
       omni_payload.push_back( tether_amount>>(8*i));
    }

    OmniScript.push_back( operation(omni_payload) );
    output Output0(0u, OmniScript); //создаем второй выход

    //выход на адрес боба
    operation::list p2pkhScript0=script::to_pay_key_hash_pattern(Bob_pubkey.to_payment_address().hash()); //скрипт для возвращает сдачу себе
    output Output1(SATOSHI_FOR_OMNI_OUTPUT, p2pkhScript0); //создаем второй выход

    //выход на адрес алисы
    operation::list p2pkhScript1=script::to_pay_key_hash_pattern(Alice_address.hash()); //скрипт для возвращает сдачу себе
    output Output2(OddMoney_satoshi, p2pkhScript1); //создаем второй выход


    redeem_tx.outputs().push_back(Output0);
    redeem_tx.outputs().push_back(Output1);
    redeem_tx.outputs().push_back(Output2);



    operation::list SwapScript;

    SwapScript.push_back( operation(opcode::ripemd160) );
    SwapScript.push_back( operation(swap_secret_hash) );
    SwapScript.push_back( operation(opcode::equal) );

    SwapScript.push_back( operation(opcode::if_) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey.point())) );

    SwapScript.push_back( operation(opcode::else_) );
    SwapScript.push_back( operation(uint32_to_data_chunk_inverse(locktime)) );
    SwapScript.push_back( operation(opcode::checklocktimeverify) );
    SwapScript.push_back( operation(opcode::drop) );
    SwapScript.push_back( operation(to_chunk(Alice_private.to_public().point() )) );

    SwapScript.push_back( operation(opcode::endif) );
    SwapScript.push_back( operation(opcode::checksig) );

    script redeem_script(SwapScript);

    //подпишемся!
    endorsement Sig0;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
    script::create_endorsement(Sig0, Alice_private.secret(), p2pkhScript1, redeem_tx, 0u, sighash_algorithm::all);

    operation::list  sig_script0;
    sig_script0.push_back(operation(Sig0));
    sig_script0.push_back(operation(to_chunk(Alice_private.to_public().point() )));
    script InputScript0(sig_script0);
    redeem_tx.inputs()[0].set_script(InputScript0);

    std::cout<<"REDEEM TRANSACTION WITHOUT BOB'S SIGN:\n"<<encode_base16( redeem_tx.to_data() )<<std::endl;

    endorsement Sig1;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
    script::create_endorsement(Sig1, Bob_private.secret(), redeem_script , redeem_tx, 1u, sighash_algorithm::all);

    operation::list  sig_script1;
    sig_script1.push_back(operation(Sig1));
    sig_script1.push_back(operation( swap_secret ));
    sig_script1.push_back( operation(to_chunk(redeem_script.to_data(false)) ));



    script InputScript1(sig_script1);
    redeem_tx.inputs()[1].set_script(InputScript1);


    return redeem_tx;

}

transaction createAtomicSwapBitcoinFundingTransaction(const ec_private& Alice_private, const data_chunk& swap_secret_hash,
                                                     const ec_compressed& Bob_pubkey, const uint32_t& amount,
                                                     const uint32_t& locktime)
{
    payment_address Alice_address=Alice_private.to_payment_address();

    transaction funding_tx;

    funding_tx.set_version(1u);


    //входы транзакции(в идеале лучше было бы, еслиб программа сама находила и строила входы, как например с помощью функции getUTXO, суть ее в том, чтобы обраться к серверу(в данном случае libbitcoin серверу) и отправить запрос на получение данных из блокчейна, из которые уже можно бы было получить UTXO для конкретного адреса,но сервер возвращает пустое множество UTXO, для любого адреса. долгое время не смогя разобраться с этой проблемой, было решено пока возложить на пользователя задачу найти и вписать свой UTXO)
    //выходы можно самостоятельно посмотреть на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //тут необходимо ввести хэш транзакции в которой есть непотраченный выход
    std::string PrevTxHash_str;
    std::cout<<"\n write your prev UTXO hash in base16, once output from this will input for channel's creation's transaction:\n";
    std::cin>>PrevTxHash_str;
    hash_digest PrevTxHash;
    decode_hash(PrevTxHash, PrevTxHash_str);

    //тут нужно ввести индекс непотраченного выхода, индексация начинается с нуля, выходы например на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //расположены сверху внизу в порядке увеличения их индекса
    uint32_t PrevTxIndex;
    std::cout<<"\n write index unspended output of your UTXO, its output will be input:\n";
    std::cin>>PrevTxIndex;

    output_point UTXO(PrevTxHash, PrevTxIndex);

    //сдача пользователю, то есть открывающая транзакция (opening_tx) имеет 2 выхода - первый, на счет с мультподписью размер отправляеных на него биткоинов равен ширине канала, второй - сдача пользователю, остаток средств которые он хочет вернуть на свой адрес, он наберет их сам, учитывая какую сумму он хочет потратить на fees
    std::string OddMoney_btc;
    uint64_t OddMoney_satoshi;
    std::cout<<"\n write odd money (in BTC), for creating p2pkh output on your address. Dont forgot about transaction's fees:\n";
    std::cin>>OddMoney_btc;
    decode_base10(OddMoney_satoshi,OddMoney_btc, btc_decimal_places);

    uint64_t prevAmount;
    std::cout<<"input prevAmount:\n";
    std::cin>>prevAmount;


    input input0;
    input0.set_previous_output(UTXO);
    input0.set_sequence(0xffffffff);

    funding_tx.inputs().push_back(input0); //добавим вход без подписи

    //создадим выходы транзакции (2 выхода:
    //первый - 1 на адрес с атомик свопом
    //второй - сдача самому себе, выход со скриптом вида p2pkh

    operation::list SwapScript;
/*
    SwapScript.push_back( operation(opcode::ripemd160) );
    SwapScript.push_back( operation(swap_secret_hash) );
    SwapScript.push_back( operation(opcode::equalverify) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey)) );
    SwapScript.push_back( operation(opcode::equal) );

    SwapScript.push_back( operation(opcode::if_) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey)) );

    SwapScript.push_back( operation(opcode::else_) );
    SwapScript.push_back( operation(uint32_to_data_chunk_inverse(locktime)) );
    SwapScript.push_back( operation(opcode::checklocktimeverify) );
    SwapScript.push_back( operation(opcode::drop) );
    SwapScript.push_back( operation(to_chunk(Alice_private.to_public().point() )) );

    SwapScript.push_back( operation(opcode::endif) );
    SwapScript.push_back( operation(opcode::checksig) );
*/

    SwapScript.push_back( operation(opcode::ripemd160) );
    SwapScript.push_back( operation(swap_secret_hash) );
    SwapScript.push_back( operation(opcode::equal) );

    SwapScript.push_back( operation(opcode::if_) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey)) );

    SwapScript.push_back( operation(opcode::else_) );
    SwapScript.push_back( operation(uint32_to_data_chunk_inverse(locktime)) );
    SwapScript.push_back( operation(opcode::checklocktimeverify) );
    SwapScript.push_back( operation(opcode::drop) );
    SwapScript.push_back( operation(to_chunk(Alice_private.to_public().point() )) );

    SwapScript.push_back( operation(opcode::endif) );
    SwapScript.push_back( operation(opcode::checksig) );

    script redeem_script(SwapScript);

    //составим p2sh выход, с хэшем скрипта redeemScript
    //OP_HASH160 <redeemScriptHash> OP_EQUAL
    //std::cout<< encode_base16(redeem_script.to_data(false) )<<std::endl;
    short_hash redeemScriptHash=bitcoin_short_hash(redeem_script.to_data(false));
    script p2sh_swapScript = script::to_pay_script_hash_pattern(redeemScriptHash);

    output Output0(amount,p2sh_swapScript); //вызываем конструктор класса output, в который передаем 1 - количество коинов, 2 - скрипт

    operation::list p2pkhScript=script::to_pay_key_hash_pattern(Alice_address.hash()); //скрипт для возвращает сдачу себе
    output Output1(OddMoney_satoshi, p2pkhScript); //создаем второй выход

    funding_tx.outputs().push_back(Output0);
    funding_tx.outputs().push_back(Output1);

    //после добавления всех входов и выходов подпишем транзакцию

    endorsement Sig0;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
  //  script::create_endorsement(Sig0, Alice_private.secret(), p2pkhScript, funding_tx, 0, sighash_algorithm::all);

    script::create_endorsement(Sig0,Alice_private.secret(),p2pkhScript,funding_tx,0,sighash_algorithm::all, script_version::unversioned, prevAmount);
    //script::verify()

    operation::list  sig_script0;
    sig_script0.push_back(operation(Sig0));
    sig_script0.push_back(operation(to_chunk(Alice_private.to_public().point() )));
    script InputScript0(sig_script0);
    funding_tx.inputs()[0].set_script(InputScript0);

    return funding_tx;

}

transaction createAtomicSwapBitcoinRedeemTransaction(const ec_private& Alice_private,
                                                     const ec_public& Bob_pubkey, const ec_private& Bob_private, const uint64_t& tether_amount,
                                                     const uint32_t& locktime, const data_chunk& swap_secret,
                                                    const transaction& Funding_tx)
{

    payment_address Alice_address=Alice_private.to_payment_address();
    data_chunk swap_secret_hash=ripemd160_hash_chunk(swap_secret);


    transaction redeem_tx;

    redeem_tx.set_version(1u);



    input input1;
    output_point FundingTxOutput(Funding_tx.hash(), 0u);
    input1.set_previous_output(FundingTxOutput);
    input1.set_sequence(0xffffffff);

    redeem_tx.inputs().push_back(input1); //добавим вход без подписи


    //выход на адрес боба
    operation::list p2pkhScript0=script::to_pay_key_hash_pattern(Bob_pubkey.to_payment_address().hash()); //скрипт для возвращает сдачу себе
    output Output1(Funding_tx.outputs()[0].value() - CLOSING_CHANNEL_FEES, p2pkhScript0); //создаем второй выход


    redeem_tx.outputs().push_back(Output1);


    //подпишемся!
    operation::list SwapScript;

    SwapScript.push_back( operation(opcode::ripemd160) );
    SwapScript.push_back( operation(swap_secret_hash) );
    SwapScript.push_back( operation(opcode::equal) );

    SwapScript.push_back( operation(opcode::if_) );

    SwapScript.push_back( operation(to_chunk(Bob_pubkey.point())) );

    SwapScript.push_back( operation(opcode::else_) );
    SwapScript.push_back( operation(uint32_to_data_chunk_inverse(locktime)) );
    SwapScript.push_back( operation(opcode::checklocktimeverify) );
    SwapScript.push_back( operation(opcode::drop) );
    SwapScript.push_back( operation(to_chunk(Alice_private.to_public().point() )) );

    SwapScript.push_back( operation(opcode::endif) );
    SwapScript.push_back( operation(opcode::checksig) );

    script redeem_script(SwapScript);


    endorsement Sig1;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
    script::create_endorsement(Sig1, Bob_private.secret(), redeem_script , redeem_tx, 0u, sighash_algorithm::all);

    operation::list  sig_script1;
    sig_script1.push_back(operation(Sig1));
    sig_script1.push_back(operation( swap_secret ));
    sig_script1.push_back( operation(to_chunk(redeem_script.to_data(false)) ));

    script InputScript1(sig_script1);
    redeem_tx.inputs()[0].set_script(InputScript1);


    return redeem_tx;

}

transaction getTestOmni(const ec_private& Alice_private)
{
    payment_address Alice_address=Alice_private.to_payment_address();

    transaction funding_tx;

    funding_tx.set_version(1u);

    //входы транзакции(в идеале лучше было бы, еслиб программа сама находила и строила входы, как например с помощью функции getUTXO, суть ее в том, чтобы обраться к серверу(в данном случае libbitcoin серверу) и отправить запрос на получение данных из блокчейна, из которые уже можно бы было получить UTXO для конкретного адреса,но сервер возвращает пустое множество UTXO, для любого адреса. долгое время не смогя разобраться с этой проблемой, было решено пока возложить на пользователя задачу найти и вписать свой UTXO)
    //выходы можно самостоятельно посмотреть на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //тут необходимо ввести хэш транзакции в которой есть непотраченный выход
    std::string PrevTxHash_str;
    std::cout<<"\n write your prev UTXO hash in base16, once output from this will input for channel's creation's transaction:\n";
    std::cin>>PrevTxHash_str;
    hash_digest PrevTxHash;
    decode_hash(PrevTxHash, PrevTxHash_str);

    //тут нужно ввести индекс непотраченного выхода, индексация начинается с нуля, выходы например на сайте
    //https://live.blockcypher.com/btc-testnet/tx/62408b1b14ce9eea82b73b543cfb0bdfc4ec118b9d50c07e6c6d75ba3c6a7b59/
    //расположены сверху внизу в порядке увеличения их индекса
    uint32_t PrevTxIndex;
    std::cout<<"\n write index unspended output of your UTXO, its output will be input:\n";
    std::cin>>PrevTxIndex;


    //сдача пользователю, то есть открывающая транзакция (opening_tx) имеет 2 выхода - первый, на счет с мультподписью размер отправляеных на него биткоинов равен ширине канала, второй - сдача пользователю, остаток средств которые он хочет вернуть на свой адрес, он наберет их сам, учитывая какую сумму он хочет потратить на fees
    std::string OddMoney_btc;
    uint64_t OddMoney_satoshi;
    std::cout<<"\n write odd money (in BTC), for creating p2pkh output on your address. Dont forgot about transaction's fees:\n";
    std::cin>>OddMoney_btc;
    decode_base10(OddMoney_satoshi,OddMoney_btc, btc_decimal_places);

    output_point UTXO(PrevTxHash, PrevTxIndex);

    input input0;
    input0.set_previous_output(UTXO);
    input0.set_sequence(0xffffffff);

    funding_tx.inputs().push_back(input0); //добавим вход без подписи

    //создадим выходы транзакции (2 выхода:
    payment_address server_address("moneyqMan7uh8FqdCA2BV5yZ8qVrc9ikLP");
    operation::list p2pkhScript0=script::to_pay_key_hash_pattern(server_address.hash()); //скрипт для возвращает сдачу себе
    output Output0(100000000u, p2pkhScript0); //создаем второй выход


    operation::list p2pkhScript1=script::to_pay_key_hash_pattern(Alice_address.hash()); //скрипт для возвращает сдачу себе
    output Output1(OddMoney_satoshi, p2pkhScript1); //создаем второй выход

    funding_tx.outputs().push_back(Output0);
    funding_tx.outputs().push_back(Output1);

    //после добавления всех входов и выходов подпишем транзакцию

    endorsement Sig0;
   // считаем скрипт выхода соответствующнго входу0 равен p2pkhScript
    script::create_endorsement(Sig0, Alice_private.secret(), p2pkhScript1, funding_tx, 0, sighash_algorithm::all);

    operation::list  sig_script0;
    sig_script0.push_back(operation(Sig0));
    sig_script0.push_back(operation(to_chunk(Alice_private.to_public().point() )));
    script InputScript0(sig_script0);
    funding_tx.inputs()[0].set_script(InputScript0);

    return funding_tx;
}



int main()
{
    transaction funding_tx;
    transaction redeem_tx;
    transaction bitcoin_funding_tx;
    transaction bitcoin_redeem_tx;


    std::string AlicePubKey_str;
    std::cout<<"enter Alice public key in base16:\n";
    std::cin>>AlicePubKey_str;
    ec_public AlicePubKey(AlicePubKey_str);

    //для подписи требуется приватный ключ пользователя
    std::string AliceSecret_str;
    std::cout<<"write Alice private key in base16 (it need for sifnaturing of tx, and doesnt be send on server):\n";
    std::cin>>AliceSecret_str;
    ec_secret AliceSecret;
    decode_base16(AliceSecret, AliceSecret_str);
    ec_private AlicePrivate(AliceSecret, ec_private::mainnet);
    if(AlicePrivate.to_public() != AlicePubKey) {std::cout<<"error: Invalid private key for inputed before public key\n";}


    std::string BoBPubKey_str;
    std::cout<<"enter BoB public key in base16:\n";
    std::cin>>BoBPubKey_str;
    ec_public BoBPubKey(BoBPubKey_str);

    //для подписи требуется приватный ключ пользователя
    std::string BoBSecret_str;
    std::cout<<"write BoB private key in base16 (it need for sifnaturing of tx, and doesnt be send on server):\n";
    std::cin>>BoBSecret_str;
    ec_secret BoBSecret;
    decode_base16(BoBSecret, BoBSecret_str);
    ec_private BoBPrivate(BoBSecret, ec_private::mainnet);
    if(BoBPrivate.to_public() != BoBPubKey) {std::cout<<"error: Invalid private key for inputed before public key\n";}


    data_chunk swap_secret(32);
    pseudo_random_fill(swap_secret);
    std::cout<<"Secret: - "<<encode_base16(swap_secret)<<std::endl;

    data_chunk swap_secret_hash=ripemd160_hash_chunk(swap_secret);
    std::cout<<"Secret Hash: - "<<encode_base16(swap_secret_hash)<<std::endl;

    uint32_t nlocktime;
    std::string Command;
    while(true)
    {
        std::cout<<"write \"-start\" for creating channel or \"-exit\"\n";
        std::cin>>Command;
        if(Command=="-exit") {break;}
        if(Command=="-start")
        {
            std::cout<<"input nlocktime:\n";
            std::cin>>nlocktime;

            bitcoin_funding_tx=createAtomicSwapBitcoinFundingTransaction(BoBPrivate,swap_secret_hash,AlicePubKey,100000u,nlocktime);

            std::cout<<"FUNDING BITCOIN TRANSACTION:\n"<<encode_base16( bitcoin_funding_tx.to_data() )<<std::endl;


            funding_tx= createAtomicSwapTetherFundingTransaction(AlicePrivate,swap_secret_hash,BoBPubKey,nlocktime);
            //funding_tx=getTestOmni(AlicePrivate);
            std::cout<<"FUNDING TRANSACTION:\n"<<encode_base16( funding_tx.to_data() )<<std::endl;

          //  validate_tx(funding_tx);
           // broadcast_tx(funding_tx);

            redeem_tx= createAtomicSwapTetherRedeemTransaction(AlicePrivate, BoBPubKey,BoBPrivate,100000000u,nlocktime,swap_secret,funding_tx);
            //funding_tx=getTestOmni(AlicePrivate);
            std::cout<<"REDEEM TRANSACTION:\n"<<encode_base16( redeem_tx.to_data() )<<std::endl;

            bitcoin_redeem_tx = createAtomicSwapBitcoinRedeemTransaction(BoBPrivate, AlicePubKey, AlicePrivate,100000u,nlocktime,swap_secret, bitcoin_funding_tx);
            std::cout<<"REDEEM BITCOIN TRANSACTION:\n"<<encode_base16( bitcoin_redeem_tx.to_data() )<<std::endl;




        }
    }
}

