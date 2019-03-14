/*-------------------------------------------------------------------------------
  This file is part of generalized random forest (grf).

  grf is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grf is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grf. If not, see <http://www.gnu.org/licenses/>.
 #-------------------------------------------------------------------------------*/

#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include "Eigen/Dense"
#include "commons/utility.h"
#include "commons/Observations.h"
#include "prediction/LLCausalPredictionStrategy.h"

const size_t LLCausalPredictionStrategy::OUTCOME = 0;
const size_t LLCausalPredictionStrategy::TREATMENT = 1;

size_t LLCausalPredictionStrategy::prediction_length() {
  return 1;
}

LLCausalPredictionStrategy::LLCausalPredictionStrategy(std::vector<double> lambdas,
                                                   bool use_unweighted_penalty,
                                                   std::vector<size_t> linear_correction_variables):
        lambdas(lambdas),
        use_unweighted_penalty(use_unweighted_penalty),
        linear_correction_variables(linear_correction_variables){
};

std::vector<double> LLCausalPredictionStrategy::predict(
        size_t sampleID,
        const std::unordered_map<size_t, double>& weights_by_sampleID,
        const Data *original_data,
        const Data *test_data) {
  size_t num_variables = linear_correction_variables.size();
  size_t num_total_predictors = original_data->get_num_cols() - 2;
  size_t num_nonzero_weights = weights_by_sampleID.size();

  std::vector<size_t> indices(num_nonzero_weights);
  Eigen::MatrixXd weights_vec = Eigen::VectorXd::Zero(num_nonzero_weights);
  {
    size_t i = 0;
    for (auto& it : weights_by_sampleID) {
      size_t index = it.first;
      double weight = it.second;
      indices[i] = index;
      weights_vec(i) = weight;
      i++;
    }
  }

  Eigen::MatrixXd X (num_nonzero_weights, num_variables + 2);
  Eigen::MatrixXd Y (num_nonzero_weights, 1);
  for (size_t i = 0; i < num_nonzero_weights; ++i) {
    // X columns
    for (size_t j = 0; j < num_variables; ++j){
      size_t current_predictor = linear_correction_variables[j];
      X(i,j+1) = test_data->get(sampleID, current_predictor)
                 - original_data->get(indices[i],current_predictor);
    }
    // treatment W column
    X(i, num_variables+1) = original_data->get(indices[i], num_total_predictors+1);

    Y(i) = original_data->get(indices[i], num_total_predictors);
    X(i, 0) = 1;
  }

  // find ridge regression predictions
  Eigen::MatrixXd M (num_variables+1, num_variables+1);
  M.noalias() = X.transpose()*weights_vec.asDiagonal()*X;

  size_t num_lambdas = lambdas.size();
  std::vector<double> predictions(num_lambdas);

  for( size_t i = 0; i < num_lambdas; ++i){
    double lambda = lambdas[i];
    if (use_unweighted_penalty) {
      // standard ridge penalty
      double normalization = M.trace() / num_variables + 1;
      for (size_t j = 1; j < num_variables+1; ++j){
        M(j,j) += lambda * normalization;
      }
    } else {
      // covariance ridge penalty
      for (size_t j = 1; j < num_variables+1; ++j){
        M(j,j) += lambda * M(j,j); // note that the weights are already normalized
      }
    }

    Eigen::MatrixXd preds = M.ldlt().solve(X.transpose()*weights_vec.asDiagonal()*Y);
    predictions[i] = preds(num_variables + 1);
  }

  return predictions;
}

std::vector<double> LLCausalPredictionStrategy::compute_variance(
        size_t sampleID,
        std::vector<std::vector<size_t>> samples_by_tree,
        std::unordered_map<size_t, double> weights_by_sampleID,
        const Data* train_data,
        const Data* data,
        size_t ci_group_size){
  return { 0.0 };
}
