#include "DoubleSelectionDualActionSecond.h"
#include <classical_messages_m.h>
#include <modules/QRSA/RuleEngine/IRuleEngine.h>

namespace quisp {
namespace rules {
namespace actions {

DoubleSelectionDualActionSecond::DoubleSelectionDualActionSecond() {}

DoubleSelectionDualActionSecond::DoubleSelectionDualActionSecond(unsigned long RuleSet_id, int rule_index, int part, QNIC_type qt, int qi, int res, int tres_X, int tres_Z,
                                                                 int ds_X) {
  partner = part;
  qnic_type = qt;
  qnic_id = qi;
  resource = res;
  trash_resource_X = tres_X;
  trash_resource_Z = tres_Z;
  doubleselection_trash_resource_X = ds_X;
  rule_id = rule_index;
  ruleset_id = RuleSet_id;
  // action_index++;
}

// X purification, Z purification to trash_qubit_X Bell pair.
cPacket *DoubleSelectionDualActionSecond::run(cModule *re) {
  IStationaryQubit *qubit = nullptr;
  IStationaryQubit *trash_qubit_Z, *trash_qubit_X, *ds_trash_qubit_X = nullptr;

  qubit = getResource_fromTop(resource);
  trash_qubit_X = getResource_fromTop(trash_resource_X);
  trash_qubit_Z = getResource_fromTop(trash_resource_Z);
  ds_trash_qubit_X = getResource_fromTop(doubleselection_trash_resource_X);

  if (qubit == trash_qubit_X || qubit == trash_qubit_Z || trash_qubit_Z == trash_qubit_X || qubit == ds_trash_qubit_X || trash_qubit_X == ds_trash_qubit_X) {
    Error *pk = new Error;
    pk->setError_text("Qubit and Trash_qubit must be different.");
    return pk;
  }
  if (qubit == nullptr || trash_qubit_Z == nullptr || trash_qubit_X == nullptr || ds_trash_qubit_X == nullptr) {
    Error *pk = new Error;
    pk->setError_text("Not enough resource (Qubit and Trash_qubit) found. This should have been checked as a condition clause.");
    return pk;
  }
  bool meas_X, meas_Z, ds_meas_X = false;

  meas_X = trash_qubit_X->Xpurify(qubit);  // Error propagation only. Not based on density matrix

  meas_Z = trash_qubit_Z->Zpurify(qubit);  // Error propagation only. Not based on density matrix
  ds_meas_X = ds_trash_qubit_X->Xpurify(trash_qubit_Z);

  qubit->Lock(ruleset_id, rule_id, action_index);

  if (trash_qubit_Z->entangled_partner != nullptr) {  // Trash qubit has been measured. Now, break the entanglement info of the partner. There is no need to overwrite its density
                                                      // matrix since we are only tracking errors.
    trash_qubit_Z->entangled_partner->no_density_matrix_nullptr_entangled_partner_ok =
        true;  // For debugging. Code in RuleEngine makes sure that any new resource is either entangled or has a density matrix stored. This is not true if the partner did a
               // purification, because this does not update the densitymatrix as all we do is track error.
    trash_qubit_Z->entangled_partner->entangled_partner = nullptr;  // Break entanglement.
  }
  if (trash_qubit_X->entangled_partner != nullptr) {  // Trash qubit has been measured. Now, break the entanglement info of the partner. There is no need to overwrite its density
                                                      // matrix since we are only tracking errors.
    trash_qubit_X->entangled_partner->no_density_matrix_nullptr_entangled_partner_ok =
        true;  // For debugging. Code in RuleEngine makes sure that any new resource is either entangled or has a density matrix stored. This is not true if the partner did a
               // purification, because this does not update the densitymatrix as all we do is track error.
    trash_qubit_X->entangled_partner->entangled_partner = nullptr;  // Break entanglement.
  }
  if (ds_trash_qubit_X->entangled_partner != nullptr) {  // Trash qubit has been measured. Now, break the entanglement info of the partner. There is no need to overwrite its
                                                         // density matrix since we are only tracking errors.
    ds_trash_qubit_X->entangled_partner->no_density_matrix_nullptr_entangled_partner_ok =
        true;  // For debugging. Code in RuleEngine makes sure that any new resource is either entangled or has a density matrix stored. This is not true if the partner did a
               // purification, because this does not update the densitymatrix as all we do is track error.
    ds_trash_qubit_X->entangled_partner->entangled_partner = nullptr;  // Break entanglement.
  }
  // Delete measured resource from the tracked list of resources.
  removeResource_fromRule(trash_qubit_X);  // Remove from resource list in this Rule.
  removeResource_fromRule(trash_qubit_Z);  // Remove from resource list in this Rule.
  removeResource_fromRule(ds_trash_qubit_X);  // Remove from resource list in this Rule.

  IRuleEngine *rule_engine = check_and_cast<IRuleEngine *>(re);
  rule_engine->freeConsumedResource(qnic_id, trash_qubit_X, qnic_type);  // Remove from entangled resource list.
  rule_engine->freeConsumedResource(qnic_id, trash_qubit_Z, qnic_type);
  rule_engine->freeConsumedResource(qnic_id, ds_trash_qubit_X, qnic_type);
  // Deleting done

  DS_DoublePurificationSecondResult *pk = new DS_DoublePurificationSecondResult;
  pk->setDestAddr(partner);
  pk->setKind(7);
  pk->setAction_index(action_index);
  pk->setRule_id(rule_id);
  pk->setRuleset_id(ruleset_id);
  pk->setXOutput_is_plus(meas_X);
  pk->setZOutput_is_plus(meas_Z);
  pk->setDS_Output_is_plus(ds_meas_X);

  pk->setEntangled_with(qubit);
  action_index++;
  return pk;
}

DoubleSelectionDualActionSecondInv::DoubleSelectionDualActionSecondInv() {}

DoubleSelectionDualActionSecondInv::DoubleSelectionDualActionSecondInv(unsigned long RuleSet_id, int rule_index, int part, QNIC_type qt, int qi, int res, int tres_X, int tres_Z,
                                                                       int ds_Z) {
  partner = part;
  qnic_type = qt;
  qnic_id = qi;
  resource = res;
  trash_resource_X = tres_X;
  trash_resource_Z = tres_Z;
  doubleselection_trash_resource_Z = ds_Z;
  rule_id = rule_index;
  ruleset_id = RuleSet_id;
  // action_index++;
}

// Double selection double error (ZX error) purification
cPacket *DoubleSelectionDualActionSecondInv::run(cModule *re) {
  IStationaryQubit *qubit = nullptr;
  IStationaryQubit *trash_qubit_Z, *trash_qubit_X, *ds_trash_qubit_Z = nullptr;

  qubit = getResource_fromTop(resource);
  trash_qubit_X = getResource_fromTop(trash_resource_X);
  trash_qubit_Z = getResource_fromTop(trash_resource_Z);
  ds_trash_qubit_Z = getResource_fromTop(doubleselection_trash_resource_Z);

  if (qubit == trash_qubit_X || qubit == trash_qubit_Z || trash_qubit_Z == trash_qubit_X || qubit == ds_trash_qubit_Z) {
    Error *pk = new Error;
    pk->setError_text("Qubit and Trash_qubit must be different.");
    return pk;
  }
  if (qubit == nullptr || trash_qubit_Z == nullptr || trash_qubit_X == nullptr || ds_trash_qubit_Z == nullptr) {
    Error *pk = new Error;
    pk->setError_text("Not enough resource (Qubit and Trash_qubit) found. This should have been checked as a condition clause.");
    return pk;
  }
  bool meas_X, meas_Z, ds_meas_Z = false;

  meas_Z = trash_qubit_Z->Zpurify(qubit);  // Error propagation only. Not based on density matrix

  meas_X = trash_qubit_X->Xpurify(qubit);  // Error propagation only. Not based on density matrix
  ds_meas_Z = ds_trash_qubit_Z->Zpurify(trash_qubit_X);

  qubit->Lock(ruleset_id, rule_id, action_index);

  if (trash_qubit_Z->entangled_partner != nullptr) {  // Trash qubit has been measured. Now, break the entanglement info of the partner. There is no need to overwrite its density
                                                      // matrix since we are only tracking errors.
    trash_qubit_Z->entangled_partner->no_density_matrix_nullptr_entangled_partner_ok =
        true;  // For debugging. Code in RuleEngine makes sure that any new resource is either entangled or has a density matrix stored. This is not true if the partner did a
               // purification, because this does not update the densitymatrix as all we do is track error.
    trash_qubit_Z->entangled_partner->entangled_partner = nullptr;  // Break entanglement.
  }
  if (trash_qubit_X->entangled_partner != nullptr) {  // Trash qubit has been measured. Now, break the entanglement info of the partner. There is no need to overwrite its density
                                                      // matrix since we are only tracking errors.
    trash_qubit_X->entangled_partner->no_density_matrix_nullptr_entangled_partner_ok =
        true;  // For debugging. Code in RuleEngine makes sure that any new resource is either entangled or has a density matrix stored. This is not true if the partner did a
               // purification, because this does not update the densitymatrix as all we do is track error.
    trash_qubit_X->entangled_partner->entangled_partner = nullptr;  // Break entanglement.
  }

  if (ds_trash_qubit_Z->entangled_partner != nullptr) {  // Trash qubit has been measured. Now, break the entanglement info of the partner. There is no need to overwrite its
                                                         // density matrix since we are only tracking errors.
    ds_trash_qubit_Z->entangled_partner->no_density_matrix_nullptr_entangled_partner_ok =
        true;  // For debugging. Code in RuleEngine makes sure that any new resource is either entangled or has a density matrix stored. This is not true if the partner did a
               // purification, because this does not update the densitymatrix as all we do is track error.
    ds_trash_qubit_Z->entangled_partner->entangled_partner = nullptr;  // Break entanglement.
  }
  // Delete measured resource from the tracked list of resources.
  removeResource_fromRule(trash_qubit_X);  // Remove from resource list in this Rule.
  removeResource_fromRule(trash_qubit_Z);  // Remove from resource list in this Rule.
  removeResource_fromRule(ds_trash_qubit_Z);  // Remove from resource list in this Rule.

  IRuleEngine *rule_engine = check_and_cast<IRuleEngine *>(re);
  rule_engine->freeConsumedResource(qnic_id, trash_qubit_X, qnic_type);  // Remove from entangled resource list.
  rule_engine->freeConsumedResource(qnic_id, trash_qubit_Z, qnic_type);
  rule_engine->freeConsumedResource(qnic_id, ds_trash_qubit_Z, qnic_type);
  // Deleting done

  DS_DoublePurificationSecondResult *pk = new DS_DoublePurificationSecondResult;
  pk->setDestAddr(partner);
  pk->setKind(7);
  pk->setAction_index(action_index);
  pk->setRule_id(rule_id);
  pk->setRuleset_id(ruleset_id);
  pk->setXOutput_is_plus(meas_X);
  pk->setZOutput_is_plus(meas_Z);
  pk->setDS_Output_is_plus(ds_meas_Z);
  pk->setEntangled_with(qubit);
  action_index++;
  return pk;
}

}  // namespace actions
}  // namespace rules
}  // namespace quisp
