// RecipeReactor.cpp
// Implements the RecipeReactor class
#include <iostream>

#include "RecipeReactor.h"

#include "Logician.h"
#include "GenException.h"
#include "InputXML.h"

/*
 * TICK
 * send a request for your capacity minus your stocks.
 * offer stocks + capacity
 *
 * TOCK
 * process as much in stocks as your capacity will allow.
 * send appropriate materials to fill ordersWaiting.
 *
 * RECIEVE MATERIAL
 * put it in stocks
 *
 * SEND MATERIAL
 * pull it from inventory
 */

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::init(xmlNodePtr cur)
{ 
  FacilityModel::init(cur);
  
  in_commod = out_commod = NULL; 
  
  // move XML pointer to current model
  cur = XMLinput->get_xpath_element(cur,"model/RecipeReactor");

  // all facilities require commodities - possibly many
  string commod_name;
  Commodity* new_commod;
  
  commod_name = XMLinput->get_xpath_content(cur,"incommodity");
  in_commod = LI->getCommodity(commod_name);
  if (NULL == in_commod)
    throw GenException("Input commodity '" + commod_name 
                       + "' does not exist for facility '" + getName() 
                       + "'.");
  
  commod_name = XMLinput->get_xpath_content(cur,"outcommodity");
  out_commod = LI->getCommodity(commod_name);
  if (NULL == out_commod)
    throw GenException("Output commodity '" + commod_name 
                       + "' does not exist for facility '" + getName() 
                       + "'.");

  inventory_size = atof(XMLinput->get_xpath_content(cur,"inventorysize"));
  capacity = atof(XMLinput->get_xpath_content(cur,"capacity"));
  lifetime = atoi(XMLinput->get_xpath_content(cur,"lifetime"));
  startConstrYr = atoi(XMLinput->get_xpath_content(cur,"startConstrYear"));
  startConstrMo = atoi(XMLinput->get_xpath_content(cur,"startConstrMonth"));
  startOpYr = atoi(XMLinput->get_xpath_content(cur,"startOperYear"));
  startOpMo = atoi(XMLinput->get_xpath_content(cur,"startOperMonth"));
  licExpYr = atoi(XMLinput->get_xpath_content(cur,"licExpYear"));
  licExpMo = atoi(XMLinput->get_xpath_content(cur,"licExpMonth"));
  state = XMLinput->get_xpath_content(cur,"state");
  typeReac = XMLinput->get_xpath_content(cur,"typeReac");
  CF = atof(XMLinput->get_xpath_content(cur,"CF"));

  inventory = deque<Material*>();
  stocks = deque<Material*>();
  ordersWaiting = deque<Message*>();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
void RecipeReactor::copy(RecipeReactor* src)
{

  FacilityModel::copy(src);

  in_commod = src->in_commod;
  out_commod = src->out_commod;
  inventory_size = src->inventory_size;
  capacity = src->capacity;
  lifetime = src->lifetime;
  startConstrYr = src->startConstrYr;
  startConstrMo = src->startConstrMo;
  startOpYr = src->startOpYr;
  startOpMo = src->startOpMo;
  licExpYr = src->licExpYr;
  licExpMo = src->licExpMo;
  state = src->state;
  typeReac = src->typeReac;
  CF = src->CF;


  inventory = deque<Material*>();
  stocks = deque<Material*>();
  ordersWaiting = deque<Message*>();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::copyFreshModel(Model* src)
{
  copy((RecipeReactor*)(src));
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::print() 
{ 
  FacilityModel::print(); 
  cout << "converts commodity {"
      << in_commod->getName()
      << "} into commodity {"
      << out_commod->getName()
      << "}, and has an inventory that holds " 
      << inventory_size << " materials"
      << endl;
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::receiveMessage(Message* msg)
{
  // is this a message from on high? 
  if(msg->getSupplierID()==this->getSN()){
    // file the order
    ordersWaiting.push_front(msg);
  }
  else {
    throw GenException("RecipeReactor is not the supplier of this msg.");
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::sendMaterial(Transaction trans, const Communicator* requester)
{
  // it should be of out_commod Commodity type
  if(trans.commod != out_commod){
    throw GenException("RecipeReactor can only send out_commod materials.");
  }

  Mass newAmt = 0;

  // pull materials off of the inventory stack until you get the trans amount

  // start with an empty manifest
  vector<Material*> toSend;

  while(trans.amount > newAmt && !inventory.empty() ){
    Material* m = inventory.front();

    // start with an empty material
    Material* newMat = new Material(CompMap(), 
                                  m->getUnits(),
                                  m->getName(), 
                                  0, atomBased);

    // if the inventory obj isn't larger than the remaining need, send it as is.
    if(m->getTotMass() <= (trans.amount - newAmt)){
      newAmt += m->getTotMass();
      newMat->absorb(m);
      inventory.pop_front();
    }
    else{ 
      // if the inventory obj is larger than the remaining need, split it.
      Material* toAbsorb = m->extractMass(trans.amount - newAmt);
      newAmt += toAbsorb->getTotMass();
      newMat->absorb(toAbsorb);
    }

    toSend.push_back(newMat);
    cout<<"RecipeReactor "<< ID
      <<"  is sending a mat with mass: "<< newMat->getTotMass()<< endl;
  }    
  ((FacilityModel*)(LI->getFacilityByID(trans.requesterID)))->receiveMaterial(trans, toSend);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::receiveMaterial(Transaction trans, vector<Material*> manifest)
{
  // grab each material object off of the manifest
  // and move it into the stocks.
  for (vector<Material*>::iterator thisMat=manifest.begin();
       thisMat != manifest.end();
       thisMat++)
  {
    cout<<"RecipeReactor " << ID << " is receiving material with mass "
        << (*thisMat)->getTotMass() << endl;
    stocks.push_back(*thisMat);
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::handleTick(int time)
{
  // MAKE A REQUEST
  // The null facility should ask for as much stuff as it can reasonably receive.
  Mass requestAmt;
  // And it can accept amounts no matter how small
  Mass minAmt = 0;
  // check how full its inventory is
  Mass inv = this->checkInventory();
  // and how much is already in its stocks
  Mass sto = this->checkStocks(); 
  // subtract inv and sto from inventory max size to get total empty space
  Mass space = inventory_size - inv - sto;
  // this will be a request for free stuff
  double commod_price = 0;

  if (space == 0){
    // don't request anything
  }
  else if (space < capacity){
    Communicator* recipient = (Communicator*)(in_commod->getMarket());
    // if empty space is less than monthly acceptance capacity
    requestAmt = space;
    // recall that requests have a negative amount
    Message* request = new Message(up, in_commod, -requestAmt, minAmt, 
                                     commod_price, this, recipient);
      // pass the message up to the inst
      (request->getInst())->receiveMessage(request);
  }
  // otherwise, the upper bound is the monthly acceptance capacity 
  // minus the amount in stocks.
  else if (space >= capacity){
    Communicator* recipient = (Communicator*)(in_commod->getMarket());
    // if empty space is more than monthly acceptance capacity
    requestAmt = capacity - sto;
    // recall that requests have a negative amount
    Message* request = new Message(up, in_commod, -requestAmt, minAmt, commod_price,
                                   this, recipient); 
    // pass the message up to the inst
    (request->getInst())->receiveMessage(request);
  }
  
  // MAKE OFFERS
  // decide how much to offer
  Mass offer_amt;
  Mass possInv = inv + capacity;

  if (possInv < inventory_size){
    offer_amt = possInv;
  }
  else {
    offer_amt = inventory_size; 
  }

  // there is no minimum amount a null facility may send
  double min_amt = 0;

  // decide what market to offer to
  Communicator* recipient = (Communicator*)(out_commod->getMarket());

  // create a message to go up to the market with these parameters
  Message* msg = new Message(up, out_commod, offer_amt, min_amt, commod_price, 
      this, recipient);

  // send it
  sendMessage(msg);
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
void RecipeReactor::handleTock(int time)
{
  // at rate allowed by capacity, convert material in Stocks to out_commod type
  // move converted material into Inventory

  Mass complete = 0;

  while(capacity > complete && !stocks.empty() ){
    Material* m = stocks.front();

    // start with an empty material
    Material* newMat = new Material(CompMap(), 
                                  m->getUnits(),
                                  m->getName(), 
                                  0, atomBased);

    // if the stocks obj isn't larger than the remaining need, send it as is.
    if(m->getTotMass() <= (capacity - complete)){
      complete += m->getTotMass();
      newMat->absorb(m);
      stocks.pop_front();
    }
    else{ 
      // if the stocks obj is larger than the remaining need, split it.
      Material* toAbsorb = m->extractMass(capacity - complete);
      complete += toAbsorb->getTotMass();
      newMat->absorb(toAbsorb);
    }

    inventory.push_back(newMat);
  }    

  // check what orders are waiting, 
  while(!ordersWaiting.empty()){
    Message* order = ordersWaiting.front();
    sendMaterial(order->getTrans(), ((Communicator*)LI->getFacilityByID(order->getRequesterID())));
    ordersWaiting.pop_front();
  }
  
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
Mass RecipeReactor::checkInventory(){
  Mass total = 0;

  // Iterate through the inventory and sum the amount of whatever
  // material unit is in each object.

  for (deque<Material*>::iterator iter = inventory.begin(); 
       iter != inventory.end(); 
       iter ++){
    total += (*iter)->getTotMass();
  }

  return total;
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    
Mass RecipeReactor::checkStocks(){
  Mass total = 0;

  // Iterate through the stocks and sum the amount of whatever
  // material unit is in each object.


  for (deque<Material*>::iterator iter = stocks.begin(); 
       iter != stocks.end(); 
       iter ++){
    total += (*iter)->getTotMass();
  }

  return total;
}

