/** \file HoM_Controller.cc
 *  \todo clean Clean code when it is simple.
 *  \todo doc Write doxygen documentation.
 *  \authors cldurand,takaakimatsuo
 *  \date 2018/04/01
 *
 *  \brief HoMController
 */
#include "../BSA/HoMController.h"

namespace quisp {
namespace modules {

Define_Module(HoMController);

HoMController::HoMController() {}

void HoMController::initialize(int stage) {
  time_out_count = 0;
  current_trial_id = dblrand();
  handshake = false;
  auto_resend_BSANotifier = true;
  BSA_timeout = 1e-6;
  address = par("address");
  receiver = par("receiver");
  passive = par("passive");
  bsa_notification_interval = par("bsa_notification_interval");
  /** \todo This code looks awefully simplifiable */
  if (passive) {
    // Nothing to do. EPPS will take care of entanglement creation.
    // max_buffer also stays unknown, until this gets a message about that info from epps.
    // Therefore, if passive, max_buffer has to be update manually every time when you get a packet from epps.
    // It still needs to know who is the neighbor of this internal hom (a.k.a qnic)
    checkNeighborAddress(true);
  } else if (receiver) {
    internodeInitializer();  // Other parameter settings
  } else if (!receiver) {
    standaloneInitializer();  // Other parameter settings
  } else {
    error("Set receiver parameter of HoM to true or false.");
  }
}

// Initialization of the HoM module inside a QNode.
// The initialization will be a little bit different from the stand-alone HoM module.
// For example, you don't need to check both of the neighbors because it is inside a QNode.
void HoMController::internodeInitializer() {
  checkNeighborAddress(true);
  checkNeighborBuffer(true);
  updateIDE_Parameter(true);

  accepted_burst_interval = (double)1 / (double)photon_detection_per_sec;
  auto *generatePacket = new HoMNotificationTimer("BsaStart");
  scheduleAt(simTime() + par("Initial_notification_timing_buffer"), generatePacket);
  // scheduleAt(simTime(),generatePacket);
}

// Initialization of the stand-alone HoM module.
void HoMController::standaloneInitializer() {
  // Just in case, check if the 2 quantum port of the node
  if (getParentModule()->gateSize("quantum_port") != 2) {
    error("No more or less than 2 neighbors are allowed for HoM.", getParentModule()->gateSize("quantum_port"));
    endSimulation();
  }
  checkNeighborAddress(false);
  checkNeighborBuffer(false);
  updateIDE_Parameter(false);

  accepted_burst_interval = (double)1 / (double)photon_detection_per_sec;
  auto *generatePacket = new HoMNotificationTimer("BsaStart");
  scheduleAt(simTime() + par("Initial_notification_timing_buffer"), generatePacket);
  // scheduleAt(simTime(),generatePacket);
}

// This is invoked only once at the begining of the simulation.
// This method sends 2 classical BSA timing notifiers to neighbors (or to itself).
// During the simulation, this method is not needed because this information is piggybacked when the node returns the results of entanglement attempt.
void HoMController::sendNotifiers() {
  double time = calculateTimeToTravel(max_neighbor_distance, speed_of_light_in_channel);  // When the packet reaches = simitme()+time
  BSMtimingNotifier *pk = generateNotifier(time, speed_of_light_in_channel, distance_to_neighbor, neighbor_address, accepted_burst_interval, photon_detection_per_sec, max_buffer);
  double first_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);
  pk->setTiming_at(first_nodes_timing);  // Tell neighboring nodes to shoot photons so that the first one arrives at BSA at the specified timing
  if (receiver) {
    pk->setInternal_qnic_index(qnic_index);
    pk->setInternal_qnic_address(qnic_address);
  }

  BSMtimingNotifier *pkt =
      generateNotifier(time, speed_of_light_in_channel, distance_to_neighbor_two, neighbor_address_two, accepted_burst_interval, photon_detection_per_sec, max_buffer);
  double second_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor_two, speed_of_light_in_channel);
  pkt->setTiming_at(second_nodes_timing);  // Tell neighboring nodes to shoot photons so that the first one arrives at BSA at the specified timing

  // If you want some uncertainty in timing calculation, maybe second_nodes_timing+uniform(-n,n) helps
  try {
    send(pk, "toRouter_port");  // send to port out. connected to local routing module (routing.localIn).
    send(pkt, "toRouter_port");
  } catch (cTerminationException &e) {
    error("Error in HoM_Controller.cc. It does not have port named toRouter_port.");
    endSimulation();
  } catch (std::exception &e) {
    error("Error in HoM_Controller.cc. It does not have port named toRouter_port.");
    endSimulation();
  }
}

void HoMController::handleMessage(cMessage *msg) {
  // std::cout<<"HoMReceiving result\n";
  // std::cout<<msg<<", bsa? ="<<(bool)( dynamic_cast<BSAresult *>(msg) != nullptr)<<"\n"; //Omnet somehow bugs without this... it receives a msg correctly from BellStateAnalyzer,
  // but very rarely does not recognize the type. VERY weird.

  if (dynamic_cast<HoMNotificationTimer *>(msg) != nullptr) {
    sendNotifiers();
    auto *notification_timer = new HoMNotificationTimer("BsaStart");
    scheduleAt(simTime() + bsa_notification_interval, notification_timer);
  } else if (dynamic_cast<BSAresult *>(msg) != nullptr) {
    // std::cout<<"BSAresult\n";
    auto_resend_BSANotifier = false;  // Photon is arriving. No need to auto reschedule next round. Wait for the last photon fron either node.
    bubble("BSAresult accumulated");
    BSAresult *pk = check_and_cast<BSAresult *>(msg);
    bool entangled = pk->getEntangled();
    // std::cout<<"Accumulating "<<entangled<<"\n";
    int prev = getStoredBSAresultsSize();
    pushToBSAresults(entangled);
    int aft = getStoredBSAresultsSize();
    if (prev + 1 != aft) {
      error("Nahnah nah!");
    }
  } else if (dynamic_cast<BSAfinish *>(msg) != nullptr) {  // Last photon from either node arrived.
    EV << "BSAfinish\n";
    bubble("BSAresult accumulated");
    BSAfinish *pk = check_and_cast<BSAfinish *>(msg);
    pushToBSAresults(pk->getEntangled());
    int stored = getStoredBSAresultsSize();
    char moge[sizeof(stored)];
    sprintf(moge, "%d", stored);
    bubble(moge);
    auto_resend_BSANotifier = true;
    current_trial_id = dblrand();
    sendBSAresultsToNeighbors();
    clearBSAresults();
  } else {
    error("what's this packet?");
  }
  delete msg;
}

// This method checks the address of the neighbors.
// If it is a receiver, meaning that it is a internode, then it checks one neighbor address and stores its own QNode address.
void HoMController::checkNeighborAddress(bool receiver) {
  if (receiver) {
    try {
      qnic_index = getParentModule()->getParentModule()->getIndex();
      qnic_address = getParentModule()->getParentModule()->par("self_qnic_address");
      neighbor_address = getQNode()->par("address");
      neighbor_address_two = getParentModule()->gate("quantum_port$o", 1)->getNextGate()->getNextGate()->getNextGate()->getOwnerModule()->par("address");
      distance_to_neighbor = getParentModule()->par("internal_distance");
      distance_to_neighbor_two = getParentModule()->gate("quantum_port$o", 1)->getNextGate()->getNextGate()->getChannel()->par("distance");
      max_neighbor_distance = std::max(distance_to_neighbor, distance_to_neighbor_two);
    } catch (std::exception &e) {
      error("Error in HoM_Controller.cc when getting neighbor addresses. Check internodeInitializer.");
      endSimulation();
    }
  } else {  // Controller in a Stand alone HoM node
    try {
      neighbor_address = getParentModule()->gate("quantum_port$o", 0)->getNextGate()->getOwnerModule()->par("address");
      neighbor_address_two = getParentModule()->gate("quantum_port$o", 1)->getNextGate()->getOwnerModule()->par("address");
      distance_to_neighbor = getParentModule()->gate("quantum_port$o", 0)->getChannel()->par("distance");
      distance_to_neighbor_two = getParentModule()->gate("quantum_port$o", 1)->getChannel()->par("distance");
      max_neighbor_distance = std::max(distance_to_neighbor, distance_to_neighbor_two);
    } catch (std::exception &e) {
      error("Error in HoM_Controller.cc. Your stand-alone HoM module may not connected to the neighboring node (quantum_port).");
      endSimulation();
    }
  }
}

// Checks the buffer size of the connected qnics.
void HoMController::checkNeighborBuffer(bool receiver) {
  if (receiver) {
    try {
      neighbor_buffer = getParentModule()->getParentModule()->par("numBuffer");
      neighbor_buffer_two = getParentModule()->gate("quantum_port$o", 1)->getNextGate()->getNextGate()->getNextGate()->getNextGate()->getOwnerModule()->par("numBuffer");
      max_buffer = std::min(neighbor_buffer, neighbor_buffer_two);  // Both nodes should transmit the same amount of photons.
    } catch (std::exception &e) {
      error("Error in HoM_Controller.cc. HoM couldnt find parameter numBuffer in the neighbor's qnic.");
      endSimulation();
    }
  } else {
    try {
      neighbor_buffer = getParentModule()->gate("quantum_port$o", 0)->getNextGate()->getNextGate()->getOwnerModule()->par("numBuffer");
      neighbor_buffer_two = getParentModule()->gate("quantum_port$o", 1)->getNextGate()->getNextGate()->getOwnerModule()->par("numBuffer");
      max_buffer = std::min(neighbor_buffer, neighbor_buffer_two);  // Both nodes should transmit the same amount of photons.
    } catch (std::exception &e) {
      error("Error in HoM_Controller.cc. HoM couldnt find parameter numBuffer in the neighbor's qnic.");
      endSimulation();
    }
  }
}

void HoMController::updateIDE_Parameter(bool receiver) {
  try {
    photon_detection_per_sec = (int)par("photon_detection_per_sec");
    if (photon_detection_per_sec <= 0) {
      error("Photon detection per sec for HoM must be more than 0.");
    }
    par("neighbor_address") = neighbor_address;
    par("neighbor_address_two") = neighbor_address_two;
    par("distance_to_neighbor") = distance_to_neighbor;
    par("distance_to_neighbor_two") = distance_to_neighbor_two;
    par("max_neighbor_distance") = max_neighbor_distance;
    par("max_buffer") = max_buffer;
    c = &par("Speed_of_light_in_fiber");
    speed_of_light_in_channel = c->doubleValue();  // per sec
    if (receiver) {
      getParentModule()->par("qnic_index") = qnic_index;
    }
  } catch (std::exception &e) {
    error(e.what());
    // std::cout<<"E="<<e.what()<<"\n";
    // error("photon_detection_per_sec is missing as a HoM_Controller parameter. Or maybe you should specify **.Speed_of_light_in_fiber = (number)km in .ini file.");
  }
}

// Generates a BSA timing notifier. This is also called only once for the same reason as sendNotifiers().
BSMtimingNotifier *HoMController::generateNotifier(double time, double speed_of_light_in_channel, double distance_to_neighbor, int destAddr, double accepted_burst_interval,
                                                   int photon_detection_per_sec, int max_buffer) {
  BSMtimingNotifier *pk = new BSMtimingNotifier("BsmTimingNotifier");
  // pk->setNumber_of_qubits(max_buffer);
  if (handshake == false)
    pk->setNumber_of_qubits(-1);  // if -1, neighbors will keep shooting photons anyway.
  else
    pk->setNumber_of_qubits(max_buffer);
  pk->setSrcAddr(getParentModule()->par("address"));  // packet source setting
  pk->setDestAddr(destAddr);
  pk->setKind(BSAtimingNotifier_type);
  pk->setInterval(accepted_burst_interval);

  // If very high, all photons can nearly be sent here(to BSA module) from neighboring nodes simultaneously
  pk->setAccepted_photons_per_sec(photon_detection_per_sec);
  double timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);

  // Tell neighboring nodes to shoot photons so that the first one arrives at BSA at the specified timing
  pk->setTiming_at(timing);
  return pk;
}

// Generates a packet that includes the BSA timing notifier and the BSA entanglement attempt results.
CombinedBSAresults *HoMController::generateNotifier_c(double time, double speed_of_light_in_channel, double distance_to_neighbor, int destAddr, double accepted_burst_interval,
                                                      int photon_detection_per_sec, int max_buffer) {
  CombinedBSAresults *pk = new CombinedBSAresults("CombinedBsaResults");
  // pk->setNumber_of_qubits(max_buffer);
  if (handshake == false)
    pk->setNumber_of_qubits(-1);  // if -1, neighbors will keep shooting photons anyway.
  else
    pk->setNumber_of_qubits(max_buffer);
  pk->setSrcAddr(getParentModule()->par("address"));  // packet source setting
  pk->setDestAddr(destAddr);
  pk->setKind(BSAtimingNotifier_type);
  pk->setInterval(accepted_burst_interval);

  // If very high, all photons can nearly be sent here(to BSA module) from neighboring nodes simultaneously
  pk->setAccepted_photons_per_sec(photon_detection_per_sec);
  double timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);

  // Tell neighboring nodes to shoot photons so that the first one arrives at BSA at the specified timing
  pk->setTiming_at(timing);
  return pk;
}

// Depending on the distance to the neighbor QNIC, this calculates when the neighbor needs to start the emission.
// The farther node emits it instantaneously, while the closer one needs to wait because 2 photons need to arrive at HoM simultaneously.
double HoMController::calculateEmissionStartTime(double time, double distance_to_node, double c) {
  // distance_to_node is the distance to HoM to self
  double self_timeToTravel = calculateTimeToTravel(distance_to_node, c);

  // If shorter distance, then the node needs to wait a little for the other node with the longer distance
  if ((self_timeToTravel) != time) {
    // Waiting time
    return (time - self_timeToTravel) * 2;
  } else {
    // No need to wait
    return 0;
  }
}

double HoMController::calculateTimeToTravel(double distance, double c) { return (distance / c); }

void HoMController::BubbleText(const char *txt) {
  if (hasGUI()) {
    char text[32];
    sprintf(text, "%s", txt);
    bubble(text);
  }
}

cModule *HoMController::getQNode() {
  // We know that Connection manager is not the QNode, so start from the parent.
  cModule *currentModule = getParentModule();
  try {
    cModuleType *QNodeType = cModuleType::get("modules.QNode");  // Assumes the node in a network has a type QNode
    while (currentModule->getModuleType() != QNodeType) {
      currentModule = currentModule->getParentModule();
    }
    return currentModule;
  } catch (std::exception &e) {
    error("No module with QNode type found. Have you changed the type name in ned file?");
    endSimulation();
  }
  return currentModule;
}

void HoMController::pushToBSAresults(bool attempt_success) {
  int prev = getStoredBSAresultsSize();
  results[getStoredBSAresultsSize()] = attempt_success;
  int aft = getStoredBSAresultsSize();
  if (prev + 1 != aft) {
    error("Not working correctly");
  }
}
int HoMController::getStoredBSAresultsSize() { return results.size(); }
void HoMController::clearBSAresults() { results.clear(); }

// Instead of sendNotifiers, we invoke this during the simulation to return the next BSA timing and the result.
// This should be simplified more.
void HoMController::sendBSAresultsToNeighbors() {
  if (!passive) {
    CombinedBSAresults *pk, *pkt;

    double time = calculateTimeToTravel(max_neighbor_distance, speed_of_light_in_channel);  // When the packet reaches = simitme()+time
    pk = generateNotifier_c(time, speed_of_light_in_channel, distance_to_neighbor, neighbor_address, accepted_burst_interval, photon_detection_per_sec, max_buffer);
    double first_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);

    // Tell neighboring nodes to shoot photons so that the first one arrives at BSA at the specified timing
    pk->setTiming_at(first_nodes_timing);
    pk->setSrcAddr(address);
    pk->setDestAddr(neighbor_address);
    pk->setList_of_failedArraySize(getStoredBSAresultsSize());
    pk->setKind(5);
    if (receiver) {
      pk->setInternal_qnic_index(qnic_index);
      pk->setInternal_qnic_address(qnic_address);
    }

    pkt = generateNotifier_c(time, speed_of_light_in_channel, distance_to_neighbor_two, neighbor_address_two, accepted_burst_interval, photon_detection_per_sec, max_buffer);
    double second_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor_two, speed_of_light_in_channel);
    pkt->setTiming_at(second_nodes_timing);  // Tell neighboring nodes to shoot photons so that the first one arrives at BSA at the specified timing
    EV << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!list of failed size = " << getStoredBSAresultsSize() << "\n";
    pkt->setSrcAddr(address);
    pkt->setDestAddr(neighbor_address_two);
    pkt->setList_of_failedArraySize(getStoredBSAresultsSize());
    pkt->setKind(5);

    for (auto it : results) {
      int index = it.first;
      bool entangled = it.second;
      // std::cout<<index<<" th, entangled = "<<entangled<<"\n";
      pk->setList_of_failed(index, entangled);
      pkt->setList_of_failed(index, entangled);
    }
    send(pk, "toRouter_port");
    send(pkt, "toRouter_port");

  } else {  // For SPDC type link
    CombinedBSAresults_epps *pk, *pkt;
    pk = new CombinedBSAresults_epps();
    pkt = new CombinedBSAresults_epps();

    pk->setSrcAddr(address);
    pk->setDestAddr(neighbor_address);
    pk->setList_of_failedArraySize(getStoredBSAresultsSize());
    pk->setKind(6);

    pkt->setSrcAddr(address);
    pkt->setDestAddr(neighbor_address_two);
    pkt->setList_of_failedArraySize(getStoredBSAresultsSize());
    pkt->setKind(6);

    EV << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!list of failed size = " << getStoredBSAresultsSize() << "\n";

    for (auto it : results) {
      int index = it.first;
      bool entangled = it.second;
      pk->setList_of_failed(index, entangled);
      pkt->setList_of_failed(index, entangled);
    }
    send(pk, "toRouter_port");
    send(pkt, "toRouter_port");
  }
}

// When the BSA is passive, it does not know how many qubits to emit (because it depends on the neighbor's).
// Therefore, the EPPS sends a classical packet that includes such information.
// When CM receives it, it will also have to update the max_buffer of HoMController, to know when the emission end and send the classical BSAresults to the neighboring EPPS.
void HoMController::setMax_buffer(int buffer) {
  Enter_Method("setMax_buffer()");
  if (!passive) {
    return;
  } else {
    max_buffer = buffer;
    par("max_buffer") = buffer;
  }
}
/*
void HoMController::finish(){

}*/

}  // namespace modules
}  // namespace quisp
