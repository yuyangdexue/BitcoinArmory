////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "BDM_Server.h"
#include "BDM_seder.h"


///////////////////////////////////////////////////////////////////////////////
void BDV_Server_Object::buildMethodMap()
{
   //registerCallback
   auto registerCallback = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (this->cb_ == nullptr || !this->cb_->isValid())
         return Arguments();

      auto&& callbackArg = args.get<BinaryDataObject>();
      auto&& retval = this->cb_->respond(move(callbackArg.toStr()));
      if (!retval.hasArgs())
         LOGINFO << "returned empty callback packet";

      return retval;
   };

   methodMap_["registerCallback"] = registerCallback;

   //goOnline
   auto goOnline = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      this->startThreads();

      Arguments retarg;
      return retarg;
   };

   methodMap_["goOnline"] = goOnline;

   //getTopBlockHeight
   auto getTopBlockHeight = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto retVal = IntType(this->getTopBlockHeight());
      Arguments retarg;
      retarg.push_back(move(retVal));
      return retarg;
   };

   methodMap_["getTopBlockHeight"] = getTopBlockHeight;

   //getHistoryPage
   auto getHistoryPage = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() < 2)
         throw runtime_error("unexpected id count");

      auto toLedgerEntryVector = []
         (vector<LedgerEntry>& leVec)->LedgerEntryVector
      {
         LedgerEntryVector lev;
         for (auto& le : leVec)
         {
            LedgerEntryData led(le.getWalletID(),
               le.getValue(), le.getBlockNum(), le.getTxHash(),
               le.getIndex(), le.getTxTime(), le.isCoinbase(),
               le.isSentToSelf(), le.isChangeBack(), 
               le.isOptInRBF(), le.isChainedZC(), le.usesWitness(),
               le.getScrAddrList());
            lev.push_back(move(led));
         }

         return lev;
      };

      auto& nextID = ids[1];

      //is it a ledger from a delegate?
      auto delegateIter = delegateMap_.find(nextID);
      if (delegateIter != delegateMap_.end())
      {
         
         auto& delegateObject = delegateIter->second;

         auto arg0 = args.get<IntType>();

         auto&& retVal = delegateObject.getHistoryPage(arg0.getVal());

         Arguments retarg;
         retarg.push_back(move(toLedgerEntryVector(retVal)));
         return retarg;
      }
      
      //or a wallet?
      auto theWallet = getWalletOrLockbox(nextID);
      if (theWallet != nullptr)
      {
         //is it an address ledger?
         if (ids.size() == 3)
         {

         }

         unsigned pageId = UINT32_MAX;
         BinaryData txHash;

         //is a page or a hash
         try
         {
            pageId = args.get<IntType>().getVal();
         }
         catch (runtime_error&)
         {
            auto&& bdo = args.get<BinaryDataObject>();
            txHash = bdo.get();
         }
         
         LedgerEntryVector resultLev;
         if (pageId != UINT32_MAX)
         {
            auto&& retVal = theWallet->getHistoryPageAsVector(pageId);
            resultLev = move(toLedgerEntryVector(retVal));
         }
         else
         {
            pageId = 0;
            while (1)
            {
               auto ledgerMap = theWallet->getHistoryPage(pageId++);
               for (auto& lePair : *ledgerMap)
               {
                  auto& leHash = lePair.second.getTxHash();
                  if (leHash == txHash)
                  {
                     auto& le = lePair.second;

                     LedgerEntryData led(le.getWalletID(),
                        le.getValue(), le.getBlockNum(), le.getTxHash(),
                        le.getIndex(), le.getTxTime(), le.isCoinbase(),
                        le.isSentToSelf(), le.isChangeBack(),
                        le.isOptInRBF(), le.isChainedZC(), le.usesWitness(),
                        le.getScrAddrList());

                     LedgerEntryVector lev;
                     lev.push_back(move(led));

                     Arguments retarg;
                     retarg.push_back(move(lev));
                     return retarg;
                  }
               }
            }
         }
         
         Arguments retarg;
         retarg.push_back(move(resultLev));
         return retarg;
      }

      throw runtime_error("invalid id");
      return Arguments();
   };

   methodMap_["getHistoryPage"] = getHistoryPage;

   //registerWallet
   auto registerWallet = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 1)
         throw runtime_error("unexpected id count");

      auto&& id = args.get<BinaryDataObject>();
      auto&& scrAddrVec = args.get<BinaryDataVector>();
      bool isNew = args.get<IntType>().getVal();

      auto retVal = 
         IntType(this->registerWallet(scrAddrVec.get(), move(id.toStr()), isNew));
      
      Arguments retarg;
      retarg.push_back(move(retVal));
      return retarg;
   };

   methodMap_["registerWallet"] = registerWallet;

   //registerLockbox
   auto registerLockbox = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 1)
         throw runtime_error("unexpected id count");

      auto&& id = args.get<BinaryDataObject>();
      auto&& scrAddrVec = args.get<BinaryDataVector>();
      bool isNew = args.get<IntType>().getVal();

      auto retVal = IntType(this->registerLockbox(
         scrAddrVec.get(), move(id.toStr()), isNew));

      Arguments retarg;
      retarg.push_back(move(retVal));
      return retarg;
   };

   methodMap_["registerLockbox"] = registerLockbox;

   //registerAddrList
   auto registerAddrList = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 1)
         throw runtime_error("unexpected id count");

      auto&& id_bdo = args.get<BinaryDataObject>();
      auto& id_bd = id_bdo.get();
      string id_str(id_bd.toCharPtr(), id_bd.getSize());

      auto&& scrAddrVec = args.get<BinaryDataVector>();

      this->registerAddrVec(id_str, scrAddrVec.get());
      auto retVal = IntType(1);

      Arguments retarg;
      retarg.push_back(move(retVal));
      return retarg;
   };

   methodMap_["registerAddrList"] = registerAddrList;

   //getLedgerDelegateForWallets
   auto getLedgerDelegateForWallets = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& ledgerdelegate = this->getLedgerDelegateForWallets();
      string id = ids[0];
      id.append("_w");

      this->delegateMap_.insert(make_pair(id, ledgerdelegate));

      Arguments retarg;
      BinaryDataObject bdo(id);
      retarg.push_back(move(bdo));
      return retarg;
   };

   methodMap_["getLedgerDelegateForWallets"] = getLedgerDelegateForWallets;

   //getLedgerDelegateForLockbox
   auto getLedgerDelegateForLockboxes = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& ledgerdelegate = this->getLedgerDelegateForLockboxes();
      string id = ids[0];
      id.append("_l");

      this->delegateMap_.insert(make_pair(id, ledgerdelegate));

      Arguments retarg;
      BinaryDataObject bdo(id);
      retarg.push_back(move(bdo));
      return retarg;
   };

   methodMap_["getLedgerDelegateForLockboxes"] = getLedgerDelegateForLockboxes;

   //getLedgerDelegateForScrAddr
   auto getLedgerDelegateForScrAddr = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());

      auto&& scrAddr = args.get<BinaryDataObject>();
      auto&& ledgerdelegate = 
         this->getLedgerDelegateForScrAddr(bdId, scrAddr.get());
      string id = scrAddr.get().toHexStr();

      this->delegateMap_.insert(make_pair(id, ledgerdelegate));

      Arguments retarg;
      BinaryDataObject bdo(id);
      retarg.push_back(move(bdo));
      return retarg;
   };

   methodMap_["getLedgerDelegateForScrAddr"] = getLedgerDelegateForScrAddr;

   //getBalancesAndCount
   auto getBalancesAndCount = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());
      shared_ptr<BtcWallet> wltPtr = nullptr;
      for (int i = 0; i < this->groups_.size(); i++)
      {
         auto wltIter = this->groups_[i].wallets_.find(bdId);
         if (wltIter != this->groups_[i].wallets_.end())
            wltPtr = wltIter->second;
      }

      if (wltPtr == nullptr)
         throw runtime_error("unknown wallet/lockbox ID");

      auto height = args.get<IntType>();
      auto balance_full = 
         IntType(wltPtr->getFullBalance());
      auto balance_spen = 
         IntType(wltPtr->getSpendableBalance(height.getVal()));
      auto balance_unco = 
         IntType(wltPtr->getUnconfirmedBalance(height.getVal()));
      auto count = IntType(wltPtr->getWltTotalTxnCount());

      Arguments retarg;
      retarg.push_back(move(balance_full));
      retarg.push_back(move(balance_spen));
      retarg.push_back(move(balance_unco));
      retarg.push_back(move(count));
      return retarg;
   };

   methodMap_["getBalancesAndCount"] = getBalancesAndCount;

   //hasHeaderWithHash
   auto hasHeaderWithHash = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 1)
         throw runtime_error("unexpected id count");

      auto&& hash = args.get<BinaryDataObject>();

      auto hasHash = 
         IntType(this->blockchain().hasHeaderWithHash(hash.get()));

      Arguments retarg;
      retarg.push_back(move(hasHash));
      return retarg;
   };

   methodMap_["hasHeaderWithHash"] = hasHeaderWithHash;

   //getSpendableTxOutListForValue
   auto getSpendableTxOutListForValue = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());
      shared_ptr<BtcWallet> wltPtr = nullptr;
      for (int i = 0; i < this->groups_.size(); i++)
      {
         auto wltIter = this->groups_[i].wallets_.find(bdId);
         if (wltIter != this->groups_[i].wallets_.end())
            wltPtr = wltIter->second;
      }

      if (wltPtr == nullptr)
         throw runtime_error("unknown wallet or lockbox ID");

      auto value = args.get<IntType>().getVal();

      auto&& utxoVec = wltPtr->getSpendableTxOutListForValue(value);

      Arguments retarg;
      auto count = IntType(utxoVec.size());
      retarg.push_back(move(count));

      for (auto& utxo : utxoVec)
      {
         UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
            move(utxo.txHash_), move(utxo.script_));

         auto&& bdser = entry.serialize();
         BinaryDataObject bdo(move(bdser));
         retarg.push_back(move(bdo));
      }

      return retarg;
   };

   methodMap_["getSpendableTxOutListForValue"] = getSpendableTxOutListForValue;

   //getSpendableZCList
   auto getSpendableZCList = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());
      shared_ptr<BtcWallet> wltPtr = nullptr;
      for (int i = 0; i < this->groups_.size(); i++)
      {
         auto wltIter = this->groups_[i].wallets_.find(bdId);
         if (wltIter != this->groups_[i].wallets_.end())
            wltPtr = wltIter->second;
      }

      if (wltPtr == nullptr)
         throw runtime_error("unknown wallet or lockbox ID");

      auto&& utxoVec = wltPtr->getSpendableTxOutListZC();

      Arguments retarg;
      auto count = IntType(utxoVec.size());
      retarg.push_back(move(count));

      for (auto& utxo : utxoVec)
      {
         UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
            move(utxo.txHash_), move(utxo.script_));

         auto&& bdser = entry.serialize();
         BinaryDataObject bdo(move(bdser));
         retarg.push_back(move(bdo));
      }

      return retarg;
   };

   methodMap_["getSpendableZCList"] = getSpendableZCList;

   //getRBFTxOutList
   auto getRBFTxOutList = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());
      shared_ptr<BtcWallet> wltPtr = nullptr;
      for (int i = 0; i < this->groups_.size(); i++)
      {
         auto wltIter = this->groups_[i].wallets_.find(bdId);
         if (wltIter != this->groups_[i].wallets_.end())
            wltPtr = wltIter->second;
      }

      if (wltPtr == nullptr)
         throw runtime_error("unknown wallet or lockbox ID");

      auto&& utxoVec = wltPtr->getRBFTxOutList();

      Arguments retarg;
      auto count = IntType(utxoVec.size());
      retarg.push_back(move(count));

      for (auto& utxo : utxoVec)
      {
         UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
            move(utxo.txHash_), move(utxo.script_));

         auto&& bdser = entry.serialize();
         BinaryDataObject bdo(move(bdser));
         retarg.push_back(move(bdo));
      }

      return retarg;
   };

   methodMap_["getRBFTxOutList"] = getRBFTxOutList;

   //getSpendableTxOutListForAddr
   auto getSpendableTxOutListForAddr = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());
      shared_ptr<BtcWallet> wltPtr = nullptr;
      for (int i = 0; i < this->groups_.size(); i++)
      {
         auto wltIter = this->groups_[i].wallets_.find(bdId);
         if (wltIter != this->groups_[i].wallets_.end())
            wltPtr = wltIter->second;
      }

      if (wltPtr == nullptr)
         throw runtime_error("unknown wallet or lockbox ID");

      auto&& scrAddr = args.get<BinaryDataObject>();      
      auto addrObj = wltPtr->getScrAddrObjByKey(scrAddr.get());

      bool ignorezc = args.get<IntType>().getVal();

      auto spentByZC = [this](const BinaryData& dbkey)->bool
      { return this->isTxOutSpentByZC(dbkey); };

      auto&& utxoVec = addrObj->getAllUTXOs(spentByZC);

      Arguments retarg;
      auto count = IntType(utxoVec.size());
      retarg.push_back(move(count));

      for (auto& utxo : utxoVec)
      {
         UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
            move(utxo.txHash_), move(utxo.script_));

         auto&& bdser = entry.serialize();
         BinaryDataObject bdo(move(bdser));
         retarg.push_back(move(bdo));
      }

      return retarg;
   };

   methodMap_["getSpendableTxOutListForAddr"] = getSpendableTxOutListForAddr;


   //broadcastZC
   auto broadcastZC = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 1)
         throw runtime_error("unexpected id count");

      auto broadcastLBD = [this](BinaryDataObject bdo)->void
      {
         this->zeroConfCont_->broadcastZC(
            bdo.get(), this->getID(), 10000);
      };
      
      auto&& rawTx = args.get<BinaryDataObject>();
      thread thr(broadcastLBD, move(rawTx));
      if (thr.joinable())
         thr.detach();

      Arguments retarg;
      return retarg;
   };

   methodMap_["broadcastZC"] = broadcastZC;

   //getAddrTxnCounts
   auto getAddrTxnCounts = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());
      shared_ptr<BtcWallet> wltPtr = nullptr;
      for (int i = 0; i < this->groups_.size(); i++)
      {
         auto wltIter = this->groups_[i].wallets_.find(bdId);
         if (wltIter != this->groups_[i].wallets_.end())
            wltPtr = wltIter->second;
      }

      if (wltPtr == nullptr)
         throw runtime_error("unknown wallet or lockbox ID");

      auto&& countMap = wltPtr->getAddrTxnCounts(updateID_);

      Arguments retarg;
      auto&& mapSize = IntType(countMap.size());
      retarg.push_back(move(mapSize));

      for (auto count : countMap)
      {
         BinaryDataObject bdo(move(count.first));
         retarg.push_back(move(bdo));
         retarg.push_back(move(IntType(count.second)));
      }

      return retarg;
   };

   methodMap_["getAddrTxnCounts"] = getAddrTxnCounts;

   //getAddrBalances
   auto getAddrBalances = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto& walletId = ids[1];
      BinaryData bdId((uint8_t*)walletId.c_str(), walletId.size());
      shared_ptr<BtcWallet> wltPtr = nullptr;
      for (int i = 0; i < this->groups_.size(); i++)
      {
         auto wltIter = this->groups_[i].wallets_.find(bdId);
         if (wltIter != this->groups_[i].wallets_.end())
            wltPtr = wltIter->second;
      }

      if (wltPtr == nullptr)
         throw runtime_error("unknown wallet or lockbox ID");

      auto&& balanceMap = wltPtr->getAddrBalances(
         updateID_, this->getTopBlockHeight());

      Arguments retarg;
      auto&& mapSize = IntType(balanceMap.size());
      retarg.push_back(move(mapSize));

      for (auto balances : balanceMap)
      {
         BinaryDataObject bdo(move(balances.first));
         retarg.push_back(move(bdo));
         retarg.push_back(move(IntType(get<0>(balances.second))));
         retarg.push_back(move(IntType(get<1>(balances.second))));
         retarg.push_back(move(IntType(get<2>(balances.second))));
      }

      return retarg;
   };

   methodMap_["getAddrBalances"] = getAddrBalances;


   //getTxByHash
   auto getTxByHash = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 1)
         throw runtime_error("unexpected id count");

      auto&& txHash = args.get<BinaryDataObject>();
      auto&& retval = this->getTxByHash(txHash.get());
      BinaryDataObject bdo(move(retval.serializeWithMetaData()));

      Arguments retarg;
      retarg.push_back(move(bdo));
      return move(retarg);
   };

   methodMap_["getTxByHash"] = getTxByHash;

   //getAddressFullBalance
   auto getAddressFullBalance = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto&& scrAddr = args.get<BinaryDataObject>();
      auto&& retval = this->getAddrFullBalance(scrAddr.get());

      Arguments retarg;
      retarg.push_back(move(IntType(get<0>(retval))));
      return move(retarg);
   };

   methodMap_["getAddressFullBalance"] = getAddressFullBalance;

   //getAddressTxioCount
   auto getAddressTxioCount = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto&& scrAddr = args.get<BinaryDataObject>();
      auto&& retval = this->getAddrFullBalance(scrAddr.get());

      Arguments retarg;
      retarg.push_back(move(IntType(get<1>(retval))));
      return move(retarg);
   };

   methodMap_["getAddressTxioCount"] = getAddressTxioCount;

   //getHeaderByHeight
   auto getHeaderByHeight = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 1)
         throw runtime_error("unexpected id count");

      auto height = args.get<IntType>().getVal();
      auto header = blockchain().getHeaderByHeight(height);

      BinaryDataObject bdo(header->serialize());

      Arguments retarg;
      retarg.push_back(move(bdo));
      return move(retarg);
   };

   methodMap_["getHeaderByHeight"] = getHeaderByHeight;

   //createAddressBook
   auto createAddressBook = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      if (ids.size() != 2)
         throw runtime_error("unexpected id count");

      auto wltPtr = getWalletOrLockbox(ids[1]);
      if (wltPtr == nullptr)
         throw runtime_error("invalid id");

      auto&& abeVec = wltPtr->createAddressBook();

      Arguments retarg;
      unsigned count = abeVec.size();
      retarg.push_back(move(IntType(count)));

      for (auto& abe : abeVec)
      {
         auto&& serString = abe.serialize();
         BinaryDataObject bdo(move(serString));
         retarg.push_back(move(bdo));
      }

      return move(retarg);
   };

   methodMap_["createAddressBook"] = createAddressBook;

   //updateWalletsLedgerFilter
   auto updateWalletsLedgerFilter = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& bdVec = args.get<BinaryDataVector>();

      this->updateWalletsLedgerFilter(bdVec.get());

      Arguments retarg;
      unsigned done = 1;
      retarg.push_back(move(IntType(done)));
      return move(retarg);
   };

   methodMap_["updateWalletsLedgerFilter"] = updateWalletsLedgerFilter;

   //getNodeStatus
   auto getNodeStatus = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& nodeStatus = this->bdmPtr_->getNodeStatus();

      BinaryDataObject bdo(nodeStatus.serialize());
      Arguments retarg;
      retarg.push_back(move(bdo));
      return move(retarg);
   };

   methodMap_["getNodeStatus"] = getNodeStatus;

   //estimateFee
   auto estimateFee = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto blocksToConfirm = args.get<IntType>().getVal();
      auto strat_bd = args.get<BinaryDataObject>().get();
      string strat(strat_bd.toCharPtr(), strat_bd.getSize());

      auto feeByte = this->bdmPtr_->nodeRPC_->getFeeByteSmart(
         blocksToConfirm, strat);

      BinaryWriter bw;
      bw.put_double(feeByte.feeByte_);
      BinaryDataObject val(bw.getData());
      IntType version(feeByte.smartFee_);
      BinaryDataObject err(feeByte.error_);

      Arguments retarg;
      retarg.push_back(move(val));
      retarg.push_back(move(version));
      retarg.push_back(move(err));
      return move(retarg);
   };

   methodMap_["estimateFee"] = estimateFee;

   //getHistoryForWalletSelection
   auto getHistoryForWalletSelection = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& wltIDs = args.get<BinaryDataVector>();
      auto orderingBdo = args.get<BinaryDataObject>().get();
      auto orderingStr = string(orderingBdo.getCharPtr(), orderingBdo.getSize());

      HistoryOrdering ordering;
      if (orderingStr == "ascending")
         ordering = order_ascending;
      else if (orderingStr == "descending")
         ordering = order_descending;
      else
         throw runtime_error("invalid ordering str");

      auto&& wltGroup = this->getStandAloneWalletGroup(wltIDs.get(), ordering);
            
      LedgerEntryVector lev;
      for (unsigned y = 0; y < wltGroup.getPageCount(); y++)
      {
         auto&& histPage = wltGroup.getHistoryPage(y, false, false, UINT32_MAX);
         for (auto& le : histPage)
         {
            LedgerEntryData led(le.getWalletID(),
               le.getValue(), le.getBlockNum(), le.getTxHash(),
               le.getIndex(), le.getTxTime(), le.isCoinbase(),
               le.isSentToSelf(), le.isChangeBack(),
               le.isOptInRBF(), le.isChainedZC(), le.usesWitness(),
               le.getScrAddrList());
            lev.push_back(move(led));
         }
      }

      Arguments retarg;
      retarg.push_back(move(lev));
      return move(retarg);
   };

   methodMap_["getHistoryForWalletSelection"] = getHistoryForWalletSelection;

   //getValueForTxOut
   auto getValueForTxOut = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& txHashObj = args.get<BinaryDataObject>();
      auto& txHash = txHashObj.get();
      auto txoutIndex = args.get<IntType>().getVal();
         
      auto&& theTx = this->getTxByHash(txHash);
      if (!theTx.isInitialized())
         throw runtime_error("failed to find tx");

      auto&& txOut = theTx.getTxOutCopy(txoutIndex);

      Arguments retarg;
      retarg.push_back(move(IntType(txOut.getValue())));
      return move(retarg);
   };

   methodMap_["getValueForTxOut"] = getValueForTxOut;

   //broadcastThroughRPC
   auto broadcastThroughRPC = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& rawTx = args.get<BinaryDataObject>();
      auto& rawTxBd = rawTx.get();

      auto&& response =
         this->bdmPtr_->nodeRPC_->broadcastTx(rawTxBd);

      if (response == "success")
      {
         this->bdmPtr_->zeroConfCont_->pushZcToParser(rawTxBd);
      }

      BinaryData response_bdr(response);

      Arguments retarg;
      retarg.push_back(move(BinaryDataObject(move(response_bdr))));
      return move(retarg);
   };

   methodMap_["broadcastThroughRPC"] = broadcastThroughRPC;

   //getUTXOsForAddrList
   auto getUTXOsForAddrList = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& addrBdVec = args.get<BinaryDataVector>();
      auto& addrVec = addrBdVec.get();

      auto&& utxoVec = this->getUnspentTxoutsForAddr160List(addrVec, false);

      Arguments retarg;
      auto count = IntType(utxoVec.size());
      retarg.push_back(move(count));

      for (auto& utxo : utxoVec)
      {
         UTXO entry(utxo.value_, utxo.txHeight_, utxo.txIndex_, utxo.txOutIndex_,
            move(utxo.txHash_), move(utxo.script_));

         auto&& bdser = entry.serialize();
         BinaryDataObject bdo(move(bdser));
         retarg.push_back(move(bdo));
      }

      return retarg;
   };

   methodMap_["getUTXOsForAddrList"] = getUTXOsForAddrList;

   //getRawHeaderForTxHash
   auto getRawHeaderForTxHash = [this]
      (const vector<string>& ids, Arguments& args)->Arguments
   {
      auto&& hash = args.get<BinaryDataObject>();
      auto&& dbKey = this->db_->getDBKeyForHash(hash.get());
      
      Arguments retarg;
      if (dbKey.getSize() == 0)
         return retarg;

      unsigned height; uint8_t dup;
      BinaryRefReader key_brr(dbKey.getRef());
      DBUtils::readBlkDataKeyNoPrefix(key_brr, height, dup);

      BinaryData rawHeader;
      try
      {
         auto block = this->blockchain().getHeaderByHeight(height);
         rawHeader = block->serialize();
      }
      catch (exception&)
      {
         return retarg;
      }

      BinaryDataObject headerBDO(move(rawHeader));
      retarg.push_back(move(headerBDO));
      return retarg;
   };

   methodMap_["getRawHeaderForTxHash"] = getRawHeaderForTxHash;

}

///////////////////////////////////////////////////////////////////////////////
const shared_ptr<BDV_Server_Object>& Clients::get(const string& id) const
{
   auto bdvmap = BDVs_.get();
   auto iter = bdvmap->find(id);
   if (iter == bdvmap->end())
   {
      LOGERR << "unknown BDVid";
      throw runtime_error("unknown BDVid");
   }

   return iter->second;
}

///////////////////////////////////////////////////////////////////////////////
Arguments Clients::processShutdownCommand(Command& cmdObj)
{
   auto& thisCookie = bdmT_->bdm()->config().cookie_;
   if (thisCookie.size() == 0)
      return Arguments();

   try
   {
      auto&& cookie = cmdObj.args_.get<BinaryDataObject>();
      auto&& cookieStr = cookie.toStr();

      if ((cookieStr.size() == 0) || (cookieStr != thisCookie))
         throw runtime_error("spawnId mismatch");
   }
   catch (...)
   {
      return Arguments();
   }

   if (cmdObj.method_ == "shutdown")
   {
      auto shutdownLambda = [this](void)->void
      {
         this->exitRequestLoop();
      };

      //run shutdown sequence in its own thread so that the fcgi listen
      //loop can exit properly.
      thread shutdownThr(shutdownLambda);
      if (shutdownThr.joinable())
         shutdownThr.detach();
   }
   else if (cmdObj.method_ == "shutdownNode")
   {
      if (bdmT_->bdm()->nodeRPC_ != nullptr)
         bdmT_->bdm()->nodeRPC_->shutdown();
   }

   return Arguments();
}

///////////////////////////////////////////////////////////////////////////////
Arguments Clients::runCommand_FCGI(const string& cmdStr)
{
   if (!run_.load(memory_order_relaxed))
      return Arguments();
   
   Command cmdObj(cmdStr);
   cmdObj.deserialize();

   if (cmdObj.method_ == "shutdown" || cmdObj.method_ == "shutdownNode")
   {
      return processShutdownCommand(cmdObj);
   }

   else if (bdmT_->bdm()->hasException())
   {
      rethrow_exception(bdmT_->bdm()->getException());
   }
   else if (cmdObj.method_ == "registerBDV")
   {
      return registerBDV(cmdObj, string());
   }
   else if (cmdObj.method_ == "unregisterBDV")
   {
      if (cmdObj.ids_.size() != 1)
         throw runtime_error("invalid arg count for unregisterBDV");

      unregisterBDV(cmdObj.ids_[0]);
      return Arguments();
   }

   //find the BDV and method
   if (cmdObj.ids_.size() == 0)
      throw runtime_error("malformed command");

   auto bdv = get(cmdObj.ids_[0]);

   //execute command
   auto&& result = bdv->executeCommand(cmdObj.method_, cmdObj.ids_, cmdObj.args_);
   bdv->resetCounter();

   return result;
}

///////////////////////////////////////////////////////////////////////////////
Arguments Clients::runCommand_WS(const uint64_t& bdvid, const string& cmdStr)
{
   Command cmdObj(cmdStr);
   cmdObj.deserialize();
   BinaryDataRef bdr((uint8_t*)&bdvid, 8);

   if (cmdObj.method_ == "shutdown" || cmdObj.method_ == "shutdownNode")
   {
      return processShutdownCommand(cmdObj);
   }

   else if (bdmT_->bdm()->hasException())
   {
      rethrow_exception(bdmT_->bdm()->getException());
   }
   else if (cmdObj.method_ == "registerBDV")
   {
      return registerBDV(cmdObj, bdr.toHexStr());
   }

   //find the BDV and method
   auto bdv = get(bdr.toHexStr());

   //execute command
   return bdv->executeCommand(cmdObj.method_, cmdObj.ids_, cmdObj.args_);
}


///////////////////////////////////////////////////////////////////////////////
void Clients::shutdown()
{
   /*shutdown sequence*/
   
   //exit BDM maintenance thread
   if (!bdmT_->shutdown())
      return;

   //shutdown ZC container
   bdmT_->bdm()->disableZeroConf();

   //terminate all ongoing scans
   bdmT_->bdm()->terminateAllScans();

   bdmT_->cleanUp();
}

///////////////////////////////////////////////////////////////////////////////
void Clients::exitRequestLoop()
{
   /*terminate request processing loop*/
   LOGINFO << "proceeding to shutdown";

   //prevent all new commands from running
   run_.store(false, memory_order_relaxed);
   
   //shutdown Clients gc thread
   gcCommands_.completed();
   
   //cleanup all BDVs
   unregisterAllBDVs();

   //shutdown node
   bdmT_->bdm()->shutdownNode();
   bdmT_->bdm()->shutdownNotifications();

   outerBDVNotifStack_.completed();
   innerBDVNotifStack_.completed();

   for (auto& thr : controlThreads_)
      if (thr.joinable())
         thr.join();

   //shutdown loop on FcgiServer side
   shutdownCallback_();
}

///////////////////////////////////////////////////////////////////////////////
void Clients::unregisterAllBDVs()
{
   auto bdvs = BDVs_.get();
   BDVs_.clear();

   for (auto& bdv : *bdvs)
      bdv.second->haltThreads();
}

///////////////////////////////////////////////////////////////////////////////
Arguments Clients::registerBDV(Command& cmd, string bdvID)
{
   try
   {
      auto&& magic_word = cmd.args_.get<BinaryDataObject>();
      auto& thisMagicWord = bdmT_->bdm()->config().magicBytes_;

      if (thisMagicWord != magic_word.get())
         throw runtime_error("");
   }
   catch (...)
   {
      throw DbErrorMsg("invalid magic word");
   }

   if(bdvID.size() == 0)
      bdvID = SecureBinaryData().GenerateRandom(10).toHexStr();

   auto newBDV = make_shared<BDV_Server_Object>(bdvID, bdmT_);

   string newID(newBDV->getID());

   //add to BDVs map
   BDVs_.insert(move(make_pair(newID, newBDV)));

   LOGINFO << "registered bdv: " << newID;

   Arguments args;
   BinaryDataObject bdo(newID);
   args.push_back(move(bdo));
   return args;
}

///////////////////////////////////////////////////////////////////////////////
void Clients::unregisterBDV(const string& bdvId)
{
   shared_ptr<BDV_Server_Object> bdvPtr;

   //shutdown bdv threads
   {
      auto bdvMap = BDVs_.get();
      auto bdvIter = bdvMap->find(bdvId);
      if (bdvIter == bdvMap->end())
         return;

      //copy shared_ptr and unregister from bdv map
      bdvPtr = bdvIter->second;
      BDVs_.erase(bdvId);
   }

   bdvPtr->haltThreads();

   //we are done
   bdvPtr.reset();
   LOGINFO << "unregistered bdv: " << bdvId;
}


///////////////////////////////////////////////////////////////////////////////
void Clients::commandThread(void) const
{
   if (bdmT_ == nullptr)
      throw runtime_error("invalid BDM thread ptr");

   while (1)
   {
      bool timedout = true;
      shared_ptr<BDV_Notification> notifPtr;

      try
      {
         notifPtr = move(bdmT_->bdm()->notificationStack_.pop_front(
            chrono::seconds(60)));
         if (notifPtr == nullptr)
            continue;
         timedout = false;
      }
      catch (StackTimedOutException&)
      {
         //nothing to do
      }
      catch (StopBlockingLoop&)
      {
         return;
      }
      catch (IsEmpty&)
      {
         LOGERR << "caught isEmpty in Clients maintenance loop";
         continue;
      }

      //trigger gc thread
      if (timedout == true || notifPtr->action_type() != BDV_Progress)
         gcCommands_.push_back(true);
      
      //don't go any futher if there is no new top
      if (timedout)
         continue;

      outerBDVNotifStack_.push_back(move(notifPtr));
   }
}

///////////////////////////////////////////////////////////////////////////////
void Clients::garbageCollectorThread(void)
{
   while (1)
   {
      try
      {
         bool command = gcCommands_.pop_front();
         if(!command)
            return;
      }
      catch (StopBlockingLoop&)
      {
         return;
      }

      vector<string> bdvToDelete;
      
      {
         auto bdvmap = BDVs_.get();

         for (auto& bdvPair : *bdvmap)
         {
            if (!bdvPair.second->cb_->isValid())
               bdvToDelete.push_back(bdvPair.first);
         }
      }

      for (auto& bdvID : bdvToDelete)
      {
         unregisterBDV(bdvID);
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
Arguments BDV_Server_Object::executeCommand(const string& method,
   const vector<string>& ids, Arguments& args)
{
   //make sure the method exists
   auto iter = methodMap_.find(method);
   if (iter == methodMap_.end())
      throw runtime_error("error: unknown method");

   return iter->second(ids, args);
}

///////////////////////////////////////////////////////////////////////////////
void BDV_Server_Object::setup()
{
   isReadyPromise_ = make_shared<promise<bool>>();
   isReadyFuture_ = isReadyPromise_->get_future();
   auto lbdFut = isReadyFuture_;

   //unsafe, should consider creating the blockchain object as a shared_ptr
   auto bc = &blockchain();

   auto isReadyLambda = [lbdFut, bc](void)->unsigned
   {
      if (lbdFut.wait_for(chrono::seconds(0)) == future_status::ready)
      {
         return bc->top()->getBlockHeight();
      }

      return UINT32_MAX;
   };

   if (BlockDataManagerConfig::getServiceType() == SERVICE_FCGI)
   {
      cb_ = make_unique<LongPoll>(isReadyLambda);
   }
   else if (BlockDataManagerConfig::getServiceType() == SERVICE_WEBSOCKET)
   {
      auto&& bdid = READHEX(getID());
      if (bdid.getSize() != 8)
         throw runtime_error("invalid bdv id");

      auto intid = (uint64_t*)bdid.getPtr();
      cb_ = make_unique<WS_Callback>(*intid);
   }
   else
   {
      throw runtime_error("unexpected service type");
   }

   buildMethodMap();
}

///////////////////////////////////////////////////////////////////////////////
BDV_Server_Object::BDV_Server_Object(
   const string& id, BlockDataManagerThread *bdmT) :
   bdvID_(id), bdmT_(bdmT), BlockDataViewer(bdmT->bdm())
{
   setup();
}

///////////////////////////////////////////////////////////////////////////////
void BDV_Server_Object::startThreads()
{
   auto initLambda = [this](void)->void
   { this->init(); };

   initT_ = thread(initLambda);
}

///////////////////////////////////////////////////////////////////////////////
void BDV_Server_Object::haltThreads()
{
   if (initT_.joinable())
      initT_.join();

   cb_->shutdown();
}

///////////////////////////////////////////////////////////////////////////////
void BDV_Server_Object::init()
{
   bdmPtr_->blockUntilReady();

   while (1)
   {
      bool isNew = false;
      set<BinaryData> scrAddrSet;
      map<string, walletRegStruct> wltMap;

      {
         unique_lock<mutex> lock(registerWalletMutex_);

         if (wltRegMap_.size() == 0)
            break;

         wltMap = move(wltRegMap_);
         wltRegMap_.clear();
      }

      //bundle addresses to register together
      for (auto& wlt : wltMap)
      {
         for (auto& scraddr : wlt.second.scrAddrVec)
            scrAddrSet.insert(scraddr);
      }

      //register address set with BDM
      auto&& waitOnFuture = bdmPtr_->registerAddressBatch(scrAddrSet, isNew);
      waitOnFuture.wait();

      //register actual wallets with BDV
      auto bdvPtr = (BlockDataViewer*)this;
      for (auto& wlt : wltMap)
      {
         //this should return when the wallet is registered, since all
         //underlying  addresses are already registered with the BDM
         if (wlt.second.type_ == TypeWallet)
            bdvPtr->registerWallet(
            wlt.second.scrAddrVec, wlt.second.IDstr, wlt.second.isNew);
         else
            bdvPtr->registerLockbox(
            wlt.second.scrAddrVec, wlt.second.IDstr, wlt.second.isNew);
      }
   }

   //could a wallet registration event get lost in between the init loop 
   //and setting the promise?

   //init wallets
   auto&& notifPtr = make_unique<BDV_Notification_Init>();
   scanWallets(move(notifPtr));

   //create zc packet and pass to wallets
   auto filterLbd = [this](const BinaryData& scrAddr)->bool
   {
      return hasScrAddress(scrAddr);
   };

   auto zcstruct = createZcNotification(filterLbd);
   scanWallets(move(zcstruct));
   
   //mark bdv object as ready
   isReadyPromise_->set_value(true);

   //callback client with BDM_Ready packet
   Arguments args;
   BinaryDataObject bdo("BDM_Ready");
   args.push_back(move(bdo));
   unsigned int topblock = blockchain().top()->getBlockHeight();
   args.push_back(move(IntType(topblock)));
   cb_->callback(move(args));
}

///////////////////////////////////////////////////////////////////////////////
void BDV_Server_Object::pushNotification(
   shared_ptr<BDV_Notification> notifPtr)
{
   auto action = notifPtr->action_type();
   if (action != BDV_Progress && action != BDV_NodeStatus)
   {
      //skip all but progress notifications if BDV isn't ready
      if (isReadyFuture_.wait_for(chrono::seconds(0)) != future_status::ready)
         return;
   }

   scanWallets(notifPtr);

   switch (action)
   {
   case BDV_NewBlock:
   {
      Arguments args2;
      auto&& payload =
         dynamic_pointer_cast<BDV_Notification_NewBlock>(notifPtr);
      uint32_t blocknum =
         payload->reorgState_.newTop_->getBlockHeight();

      BinaryDataObject bdo("NewBlock");
      args2.push_back(move(bdo));
      args2.push_back(move(IntType(blocknum)));
      cb_->callback(move(args2), OrderNewBlock);
      break;
   }

   case BDV_Refresh:
   {
      auto&& payload =
         dynamic_pointer_cast<BDV_Notification_Refresh>(notifPtr);

      IntType refreshType(payload->refresh_);
      BinaryData bdId = payload->refreshID_;
      BinaryDataVector bdvec;
      bdvec.push_back(move(bdId));

      Arguments args2;
      BinaryDataObject bdo("BDV_Refresh");
      args2.push_back(move(bdo));
      args2.push_back(move(refreshType));
      args2.push_back(move(bdvec));
      cb_->callback(move(args2), OrderRefresh);
      break;
   }

   case BDV_ZC:
   {
      Arguments args2;

      auto&& payload =
         dynamic_pointer_cast<BDV_Notification_ZC>(notifPtr);

      BinaryDataObject bdo("BDV_ZC");
      args2.push_back(move(bdo));

      LedgerEntryVector lev;
      for (auto& lePair : payload->leMap_)
      {
         auto&& le = lePair.second;
         LedgerEntryData led(le.getWalletID(),
            le.getValue(), le.getBlockNum(), move(le.getTxHash()),
            le.getIndex(), le.getTxTime(), le.isCoinbase(),
            le.isSentToSelf(), le.isChangeBack(),
            le.isOptInRBF(), le.isChainedZC(), le.usesWitness(),
            le.getScrAddrList());

         lev.push_back(move(led));
      }

      args2.push_back(move(lev));

      cb_->callback(move(args2), OrderZC);
      break;
   }

   case BDV_Progress:
   {
      auto&& payload =
         dynamic_pointer_cast<BDV_Notification_Progress>(notifPtr);

      ProgressData pd(payload->phase_, payload->progress_,
         payload->time_, payload->numericProgress_, payload->walletIDs_);

      Arguments args2;
      BinaryDataObject bdo("BDV_Progress");
      args2.push_back(move(bdo));
      args2.push_back(move(pd));

      cb_->callback(move(args2), OrderProgress);
      break;
   }

   case BDV_NodeStatus:
   {
      auto&& payload =
         dynamic_pointer_cast<BDV_Notification_NodeStatus>(notifPtr);

      Arguments args2;
      BinaryDataObject bdo("BDV_NodeStatus");
      BinaryDataObject nssBdo(payload->status_.serialize());
      args2.push_back(move(bdo));
      args2.push_back(move(nssBdo));

      cb_->callback(move(args2), OrderNodeStatus);
      break;
   }

   case BDV_Error:
   {
      auto&& payload =
         dynamic_pointer_cast<BDV_Notification_Error>(notifPtr);

      Arguments args2;
      BinaryDataObject bdo("BDV_Error");
      BinaryDataObject errBdo(payload->errStruct.serialize());
      args2.push_back(move(bdo));
      args2.push_back(move(errBdo));

      cb_->callback(move(args2), OrderBDVError);
      break;
   }

   default:
      return;
   }
}

///////////////////////////////////////////////////////////////////////////////
bool BDV_Server_Object::registerWallet(
   vector<BinaryData> const& scrAddrVec, string IDstr, bool wltIsNew)
{
   if (isReadyFuture_.wait_for(chrono::seconds(0)) != future_status::ready)
   {
      //only run this code if the bdv maintenance thread hasn't started yet
      unique_lock<mutex> lock(registerWalletMutex_);

      //save data
      auto& wltregstruct = wltRegMap_[IDstr];

      wltregstruct.scrAddrVec = scrAddrVec;
      wltregstruct.IDstr = IDstr;
      wltregstruct.isNew = wltIsNew;
      wltregstruct.type_ = TypeWallet;

      return true;
   }

   //register wallet with BDV
   auto bdvPtr = (BlockDataViewer*)this;
   return bdvPtr->registerWallet(scrAddrVec, IDstr, wltIsNew);
}

///////////////////////////////////////////////////////////////////////////////
void BDV_Server_Object::registerAddrVec(
   const string& idStr,
   vector<BinaryData> const& scrAddrVec)
{
   if (isReadyFuture_.wait_for(chrono::seconds(0)) != future_status::ready)
   {
      //only run this code if the bdv maintenance thread hasn't started yet
      unique_lock<mutex> lock(registerWalletMutex_);

      //save data
      auto& wltregstruct = wltRegMap_[idStr];

      wltregstruct.scrAddrVec = scrAddrVec;
      wltregstruct.IDstr = idStr;
      wltregstruct.isNew = false;
      wltregstruct.type_ = TypeWallet;

      return;
   }

   //register wallet with BDV
   registerArbitraryAddressVec(scrAddrVec, idStr);
}

///////////////////////////////////////////////////////////////////////////////
bool BDV_Server_Object::registerLockbox(
   vector<BinaryData> const& scrAddrVec, string IDstr, bool wltIsNew)
{
   if (isReadyFuture_.wait_for(chrono::seconds(0)) != future_status::ready)
   {
      //only run this code if the bdv maintenance thread hasn't started yet

      unique_lock<mutex> lock(registerWalletMutex_);

      //save data
      auto& wltregstruct = wltRegMap_[IDstr];

      wltregstruct.scrAddrVec = scrAddrVec;
      wltregstruct.IDstr = IDstr;
      wltregstruct.isNew = wltIsNew;
      wltregstruct.type_ = TypeLockbox;

      return true;
   }

   //register wallet with BDV
   auto bdvPtr = (BlockDataViewer*)this;
   return bdvPtr->registerLockbox(scrAddrVec, IDstr, wltIsNew);
}

///////////////////////////////////////////////////////////////////////////////
void Clients::init(BlockDataManagerThread* bdmT,
   function<void(void)> shutdownLambda)
{
   bdmT_ = bdmT;
   shutdownCallback_ = shutdownLambda;

   run_.store(true, memory_order_relaxed);

   auto mainthread = [this](void)->void
   {
      commandThread();
   };

   auto outerthread = [this](void)->void
   {
      bdvMaintenanceLoop();
   };

   auto innerthread = [this](void)->void
   {
      bdvMaintenanceThread();
   };

   auto gcThread = [this](void)->void
   {
      garbageCollectorThread();
   };

   controlThreads_.push_back(thread(mainthread));
   controlThreads_.push_back(thread(outerthread));

   unsigned innerThreadCount = 2;
   if (BlockDataManagerConfig::getDbType() == ARMORY_DB_SUPER &&
      bdmT_->bdm()->config().nodeType_ != Node_UnitTest)
      innerThreadCount = thread::hardware_concurrency();
   for (unsigned i = 0; i < innerThreadCount; i++)
      controlThreads_.push_back(thread(innerthread));

   auto callbackPtr = make_unique<ZeroConfCallbacks_BDV>(this);
   bdmT_->bdm()->registerZcCallbacks(move(callbackPtr));

   //no gc for unit tests
   if (bdmT_->bdm()->config().nodeType_ == Node_UnitTest)
      return;

   controlThreads_.push_back(thread(gcThread));
}

///////////////////////////////////////////////////////////////////////////////
void Clients::bdvMaintenanceLoop()
{
   while (1)
   {
      shared_ptr<BDV_Notification> notifPtr;
      try
      {
         notifPtr = move(outerBDVNotifStack_.pop_front());
      }
      catch (StopBlockingLoop&)
      {
         LOGINFO << "Shutting down BDV event loop";
         break;
      }

      auto bdvMap = BDVs_.get();
      auto& bdvID = notifPtr->bdvID();
      if (bdvID.size() == 0)
      {
         //empty bdvID means broadcast notification to all BDVs
         for (auto& bdv_pair : *bdvMap)
         {
            auto notifPacket = make_shared<BDV_Notification_Packet>();
            notifPacket->bdvPtr_ = bdv_pair.second;
            notifPacket->notifPtr_ = notifPtr;
            innerBDVNotifStack_.push_back(move(notifPacket));
         }
      }
      else
      {
         //grab bdv
         auto iter = bdvMap->find(bdvID);
         if (iter == bdvMap->end())
            continue;

         auto notifPacket = make_shared<BDV_Notification_Packet>();
         notifPacket->bdvPtr_ = iter->second;
         notifPacket->notifPtr_ = notifPtr;
         innerBDVNotifStack_.push_back(move(notifPacket));
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
void Clients::bdvMaintenanceThread()
{
   while (1)
   {
      shared_ptr<BDV_Notification_Packet> notifPtr;
      try
      {
         notifPtr = move(innerBDVNotifStack_.pop_front());
      }
      catch (StopBlockingLoop&)
      {
         break;
      }

      if (notifPtr->bdvPtr_ == nullptr)
      {
         LOGWARN << "null bdvPtr in notification";
         continue;
      }

      notifPtr->bdvPtr_->pushNotification(notifPtr->notifPtr_);
   }
}

///////////////////////////////////////////////////////////////////////////////
Arguments LongPoll::respond(const string& command)
{
   unique_lock<mutex> lock(mu_, defer_lock);

   if (!lock.try_lock())
   {
      Arguments arg;
      BinaryDataObject bdo("continue");
      arg.push_back(move(bdo));
      return move(arg);
   }

   count_ = 0;

   if (command == "waitOnBDV")
   {
      //test if ready
      auto topheight = isReady_();
      if (topheight != UINT32_MAX)
      {
         Arguments arg;
         BinaryDataObject bdo("BDM_Ready");
         arg.push_back(move(bdo));
         arg.push_back(move(IntType(topheight)));
         return move(arg);
      }

      //otherwise wait on callback stack as usual
   }
   else if (command != "getStatus")
   {
      //throw unknown command error
   }

   vector<Callback::OrderStruct> orderVec;

   try
   {
      orderVec = move(cbStack_.pop_all(std::chrono::seconds(50)));
   }
   catch (IsEmpty&)
   {
      Arguments arg;
      BinaryDataObject bdo("continue");
      arg.push_back(move(bdo));
      return move(arg);
   }
   catch (StackTimedOutException&)
   {
      Arguments arg;
      BinaryDataObject bdo("continue");
      arg.push_back(move(bdo));
      return move(arg);
   }
   catch (StopBlockingLoop&)
   {
      count_ = 5;

      //return terminate packet
      Callback::OrderStruct terminateOrder;
      BinaryDataObject bdo("terminate");
      terminateOrder.order_.push_back(move(bdo));
      terminateOrder.otype_ = OrderOther;
   }

   return respond_inner(orderVec);
}

///////////////////////////////////////////////////////////////////////////////
void WS_Callback::callback(Arguments&& arg, OrderType type)
{
   OrderStruct order(move(arg), type);
   vector<Callback::OrderStruct> orderVec;
   orderVec.push_back(move(order));

   //process callback
   auto&& result = respond_inner(orderVec);

   //write to socket
   WebSocketServer::write(bdvID_, WEBSOCKET_CALLBACK_ID, result);
}

///////////////////////////////////////////////////////////////////////////////
SocketCallback::~SocketCallback(void)
{}

///////////////////////////////////////////////////////////////////////////////
Arguments SocketCallback::respond_inner(vector<Callback::OrderStruct>& orderVec)
{
   //consolidate NewBlock and Refresh notifications
   Arguments* refreshOrderPtr = nullptr;
   int32_t newBlock = -1;

   Arguments arg;
   for (auto& order : orderVec)
   {
      switch (order.otype_)
      {
      case OrderNewBlock:
      {
         auto& argVector = order.order_.getArgVector();
         if (argVector.size() != 2)
            break;

         auto heightPtr = (DataObject<IntType>*)argVector[1].get();

         int blockheight = (int)heightPtr->getObj().getVal();
         if (blockheight > newBlock)
            newBlock = blockheight;

         break;
      }

      case OrderRefresh:
      {
         refreshOrderPtr = &order.order_;
         break;
      }

      case OrderTerminate:
      {
         Arguments terminateArg;
         BinaryDataObject bdo("terminate");
         terminateArg.push_back(move(bdo));
         return terminateArg;
      }

      default:
         arg.merge(order.order_);
      }
   }

   if (refreshOrderPtr != nullptr)
      arg.merge(*refreshOrderPtr);

   if (newBlock > -1)
   {
      BinaryDataObject bdo("NewBlock");
      arg.push_back(move(bdo));
      arg.push_back(move(IntType(newBlock)));
   }

   //send it
   return move(arg);
}

///////////////////////////////////////////////////////////////////////////////
set<string> ZeroConfCallbacks_BDV::hasScrAddr(const BinaryDataRef& addr) const
{
   set<string> result;
   auto bdvPtr = clientsPtr_->BDVs_.get();

   for (auto& bdv_pair : *bdvPtr)
   {
      if (bdv_pair.second->hasScrAddress(addr))
         result.insert(bdv_pair.first);
   }

   return result;
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfCallbacks_BDV::pushZcNotification(
   ZeroConfContainer::NotificationPacket& packet)
{
   auto bdvPtr = clientsPtr_->BDVs_.get();
   auto iter = bdvPtr->find(packet.bdvID_);
   if (iter == bdvPtr->end())
   {
      LOGWARN << "pushed zc notification with invalid bdvid";
      return;
   }

   auto notifPacket = make_shared<BDV_Notification_Packet>();
   notifPacket->bdvPtr_ = iter->second;
   notifPacket->notifPtr_ = make_shared<BDV_Notification_ZC>(packet);
   clientsPtr_->innerBDVNotifStack_.push_back(move(notifPacket));
}

///////////////////////////////////////////////////////////////////////////////
void ZeroConfCallbacks_BDV::errorCallback(
   const string& bdvId, string& errorStr, const string& txHash)
{
   auto bdvPtr = clientsPtr_->BDVs_.get();
   auto iter = bdvPtr->find(bdvId);
   if (iter == bdvPtr->end())
   {
      LOGWARN << "pushed zc error for missing bdv";
      return;
   }

   auto notifPacket = make_shared<BDV_Notification_Packet>();
   notifPacket->bdvPtr_ = iter->second;
   notifPacket->notifPtr_ = make_shared<BDV_Notification_Error>(
      bdvId, Error_ZC, move(errorStr), txHash);
   clientsPtr_->innerBDVNotifStack_.push_back(move(notifPacket));
}
