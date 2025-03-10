/** \file StationaryQubit.h
 *  \authors cldurand,takaakimatsuo
 *  \date 2018/03/14
 *
 *  \brief StationaryQubit
 */
#ifndef QUISP_MODULES_StationaryQubit_H_
#define QUISP_MODULES_StationaryQubit_H_

#include <PhotonicQubit_m.h>
#include "IStationaryQubit.h"

namespace quisp {
namespace modules {

#define STATIONARYQUBIT_PULSE_BEGIN 0x01
#define STATIONARYQUBIT_PULSE_END 0x02
#define STATIONARYQUBIT_PULSE_BOUND (STATIONARYQUBIT_PULSE_BEGIN | STATIONARYQUBIT_PULSE_END)

/** \class StationaryQubit StationaryQubit.h
 *
 *  \brief StationaryQubit.
 *  \ref https://arxiv.org/abs/1908.10758
 */

typedef std::complex<double> Complex;

class StationaryQubit : public IStationaryQubit {
 public:
  bool checkBusy() override;
  void setFree(bool consumed) override;
  /*In use. E.g. waiting for purification result.*/
  void Lock(unsigned long rs_id, int rule_id, int action_id) override;
  void Unlock() override;
  bool isLocked() override;
  void Allocate() override;
  void Deallocate() override;
  bool isAllocated() override;

  /**
   * \brief Emit photon.
   * \param pulse is 1 for the beginning of the burst, 2 for the end.
   */
  void emitPhoton(int pulse) override;

  /**
   * \brief Single Qubit X measurement.
   * \param This is only for simulating error propagations.
   * New errors only occur when wrong measurement result is delivered for feed-forward
   * (The error on the measured qubit propagates to the byproduct gate target qubit).
   */
  virtual quisp::types::MeasureXResult measure_X() override;

  /**
   * \brief Single Qubit Y measurement.
   * This is only for simulating error propagations.
   * New errors only occur when wrong measurement result is delivered for feed-forward
   * (The error on the measured qubit propagates to the byproduct gate target qubit).
   */
  virtual types::MeasureYResult measure_Y() override;

  /**
   * \brief Single Qubit Z measurement.
   * This is only for simulating error propagations.
   * New errors only occur when wrong measurement result is delivered for feed-forward
   * (The error on the measured qubit propagates to the byproduct gate target qubit).
   */
  virtual types::MeasureZResult measure_Z() override;

  /**
   * Performs measurement and returns +(true) or -(false) based on the density matrix of the state. Used for tomography.
   * */
  // virtual std::bitset<1> measure_density(char basis_this_qubit);/*Simultaneous dm calculation*/
  virtual measurement_outcome measure_density_independent() override; /*Separate dm calculation*/

  /**
   * \brief Two qubit CNOT gate.
   * \param Need to specify the control qubit as an argument.
   */
  void CNOT_gate(IStationaryQubit *control_qubit) override;

  /**
   * \brief Single qubit Hadamard gate
   * \param X error transforms to Z, and vise-versa.
   */
  void Hadamard_gate() override;
  void Z_gate() override;
  void X_gate() override;
  bool Xpurify(IStationaryQubit *resource_qubit) override;
  bool Zpurify(IStationaryQubit *resource_qubit) override;

  /*GOD parameters*/
  void setEntangledPartnerInfo(IStationaryQubit *partner) override;
  void setCompletelyMixedDensityMatrix() override;
  void setRelaxedDensityMatrix();
  void setExcitedDensityMatrix();
  void addXerror() override;
  void addZerror() override;

  double emission_success_probability;

  SingleGateErrorModel Hgate_error;
  SingleGateErrorModel Xgate_error;
  SingleGateErrorModel Zgate_error;
  TwoQubitGateErrorModel CNOTgate_error;
  SingleGateErrorModel Measurement_error;
  memory_error_model memory_err;
  double memory_error_rate;
  double memory_No_error_ceil;
  double memory_X_error_ceil;
  double memory_Y_error_ceil;
  double memory_Z_error_ceil;
  double memory_Excitation_error_ceil;
  double memory_Relaxation_error_ceil;

  single_qubit_error Pauli;
  measurement_operators meas_op;
  // https://arxiv.org/abs/1908.10758 Eq 5.2
  Eigen::MatrixXd Memory_Transition_matrix; /*I,X,Y,Z,Ex,Rl for single qubit. Unit in μs.*/
  Eigen::MatrixXd Memory_Transition_matrix_ns; /*I,X,Y,Z,Ex,Rl for single qubit. Unit in ns.*/
  Eigen::MatrixXd Memory_Transition_matrix_ms; /*I,X,Y,Z,Ex,Rl for single qubit. Unit in ns.*/
  int num_purified;

  bool locked;
  unsigned long locked_ruleset_id;
  int locked_rule_id;

 private:
  bool allocated;

 protected:
  void initialize() override;
  void finish() override;
  void handleMessage(cMessage *msg) override;
  messages::PhotonicQubit *generateEntangledPhoton();
  void setBusy();
  // returns the matrix that represents the errors on the Bell pair. (e.g. XY, XZ and ZI...)
  Eigen::Matrix2cd getErrorMatrix(StationaryQubit *qubit);
  // returns the dm of the physical Bell pair. Used for tomography.
  quantum_state getQuantumState();
  measurement_operator Random_Measurement_Basis_Selection();
  void setSingleQubitGateErrorModel(SingleGateErrorModel &model, std::string gate_name);
  void setTwoQubitGateErrorCeilings(TwoQubitGateErrorModel &model, std::string gate_name);
  /*Applies memory error to the given qubit*/
  void applyMemoryError();

  void applySingleQubitGateError(SingleGateErrorModel const &err);
  void applyTwoQubitGateError(TwoQubitGateErrorModel const &err, StationaryQubit *another_qubit);
};

}  // namespace modules
}  // namespace quisp

#endif /* QUISP_MODULES_StationaryQubit_H_ */
