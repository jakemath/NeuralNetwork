//
//  neural_network.cpp
//  neural_net
//
//  Created by Jacob Mathai on 11/9/19.
//  Copyright © 2019 Jacob Mathai. All rights reserved.
//

#include "../header/neural_network.h"
#include "../header/dataset_utils.h"

NeuralNetwork::NeuralNetwork(vector<unsigned short int>& layer_sizes,
                             bool random,
                             vector<double>& biases,
                             double weights_mean,
                             double weights_std) {
    unsigned layer;
    assert(layer_sizes.size() - 1 == biases.size());
    for (const auto& layer_ : layer_sizes)
        assert(layer_ > 0);
    layers = vector<Layer>(layer_sizes.size());
    for (layer = 0; layer < layer_sizes.size() - 1; ++layer)
        layers[layer] = Layer(
            layer_sizes[layer],
            random,
            biases[layer],
            layer_sizes[layer + 1],
            weights_mean,
            weights_std);
    layers.back() = Layer(layer_sizes.back(), random, 0, 0.0, 0.0, 0); // Output layer has no weights out
}

double relu(double z) { return fmax(0, z); }

double relu_derivative(double activated_z) { return activated_z > 0; }

double sigmoid(double z) { return (1.0 / (1.0 + exp(-1.0 * z))); }

double sigmoid_derivative(double activated_z) { return activated_z * (1 - activated_z); }

double tanh_derivative(double activated_z) { return 1.0 - pow(tanh(activated_z), 2); }

double none_function(double z) { return z; }

double none_derivative(double z) { return 1; }

void classify(vector<double>& prediction) {
    if (prediction.size() == 1)
        prediction[0] = (prediction[0] >= 0.5);
    else {
        double max = prediction[0];
        prediction[0] = 1;
        unsigned short int max_index = 0;
        for (unsigned short int i = 1; i < prediction.size(); ++i) {
            if (prediction[i] > max) {
                prediction[max_index] = 0;
                max_index = i;
                max = prediction[i];
                prediction[i] = 1;
            }
            else
                prediction[i] = 0;
        }
    }
}


bool NeuralNetwork::train(list<Point>& dataset,
                          string transfer_function,
                          double lr,
                          string dataset_type,
                          bool normalize_lr) {
    list<Point>::iterator datapoint = dataset.begin();
    assert(datapoint->x.size() == layers[0].size());
    unsigned i = 1, iter = 0;
    double total_cost = 0.0, cost = 0, average_cost = 1;
    std::ofstream out("../../costs/bank_cost.txt");
    while (average_cost >= .001 && !isnan(average_cost)) {
        for (datapoint = dataset.begin(); datapoint != dataset.end() && average_cost >= .001; ++datapoint, ++i) {
            cout << "ITERATION: " << iter + i;
            forward_propagate(*datapoint, transfer_function);
            cost = backpropagate(*datapoint, transfer_function);
            total_cost += abs(cost);
            average_cost = total_cost / i;
            cout << ", COST: " << average_cost << endl;
            update_weights(*datapoint, lr, normalize_lr);
            out << i << " " << average_cost << endl;
        }
        if (!isnan(average_cost) && average_cost >= .001 && dataset_type != "none" && dataset_type != "max_index_const") {
            cout << "GENERATING NEW DATASET" << endl;
            dataset = generate_dataset(500000, dataset.front().x.size(), dataset.front().y.size(), dataset_type);
        }
        else if (i < 250000) {
            average_cost = 1;
            total_cost = 0;
        }
    }
    out.close();
    cout << *this << std::flush;
    if (!isnan(cost)) {
        cout << "WEIGHTS TRAINED - MAKE PREDICTIONS" << endl;
        return true;
    }
    return false;
}


std::tuple<double (*)(double), double (*)(double)> get_transfer_methods(string transfer_function) {
    double (*transfer_fn)(double), (*transfer_derivative)(double);
    if (transfer_function == "tanh" || transfer_function == "tanh_regression") {
        transfer_fn = tanh;
        transfer_derivative = tanh_derivative;
    }
    else if (transfer_function == "sigmoid" || transfer_function == "sigmoid_regression") {
        transfer_fn = sigmoid;
        transfer_derivative = sigmoid_derivative;
    }
    else if (transfer_function == "relu" || transfer_function == "relu_regression") {
        transfer_fn = relu;
        transfer_derivative = relu_derivative;
    }
    else if (transfer_function == "none") {
        transfer_fn = none_function;
        transfer_derivative = none_derivative;
    }
    else {
        cout << "INVALID ACTIVATION FUNCTION" << endl;
        exit(1);
    }
    return std::make_tuple<double (*)(double), double (*)(double)>(*transfer_fn, *transfer_derivative);
}


vector<double> NeuralNetwork::forward_propagate(Point& datapoint, string transfer_function) {
    std::tuple<double (*)(double), double (*)(double)> transfer = get_transfer_methods(transfer_function);
    double (*transfer_fn)(double) = std::get<0>(transfer),
           (*transfer_derivative)(double) = std::get<1>(transfer);
    unsigned short int neuron, weight;
    double activated_value, transfer_value;
    vector<double> inputs(datapoint.x), new_inputs, weights;
    for (neuron = 0; neuron < datapoint.x.size(); ++neuron) { // Initialize input layer with values and activated values
        layers[0].neurons[neuron].activated_value = datapoint.x[neuron];
        layers[0].neurons[neuron].transfer_value = transfer_fn(datapoint.x[neuron]);
    }
    for (vector<Layer>::iterator layer = layers.begin(); layer != layers.end() - 1; ++layer) {
        new_inputs = vector<double>((layer + 1)->size());
        weights = vector<double>(layer->size());
        for (weight = 0; weight < (layer + 1)->size(); ++weight) {
            for (neuron = 0; neuron < layer->neurons.size(); ++neuron)
                weights[neuron] = layer->neurons[neuron].weights_to_next_layer[weight];
            activated_value = std::inner_product(weights.begin(), weights.end(), inputs.begin(), (layer + 1)->bias);
            transfer_value = transfer_fn(activated_value);
            if (transfer_function != "none"
                    && transfer_function != "tanh_regression"
                    && transfer_function != "sigmoid_regression"
                    && transfer_function != "relu_regression")
                (layer + 1)->neurons[weight].activated_value = activated_value;
            else
                (layer + 1)->neurons[weight].activated_value = transfer_value;
            (layer + 1)->neurons[weight].transfer_value = transfer_value;
            new_inputs[weight] = transfer_value;
        }
        inputs = new_inputs;
    }
    return inputs;
}

double NeuralNetwork::backpropagate(Point& datapoint, string transfer_function) {
    std::tuple<double (*)(double), double (*)(double)> transfer = get_transfer_methods(transfer_function);
    double (*transfer_fn)(double) = std::get<0>(transfer),
           (*transfer_derivative)(double) = std::get<1>(transfer);
    unsigned neuron;
    double diff, square_diff, error, weighted_error;
    vector<double> errors(layers.back().size()), new_errors;
    for (neuron = 0; neuron < layers.back().size(); ++neuron) {
        diff = datapoint.y[neuron] - layers.back().neurons[neuron].transfer_value;
        square_diff = pow(diff, 2);
        error = square_diff * transfer_derivative(layers.back().neurons[neuron].transfer_value);
        layers.back().neurons[neuron].error = error;
        errors[neuron] = error;
    }
    double cost = std::accumulate(errors.begin(), errors.end(), 0.0);
    for (vector<Layer>::reverse_iterator layer = layers.rbegin(); layer != layers.rend() - 1; ++layer) {
        new_errors = vector<double>((layer + 1)->size());
        for (neuron = 0; neuron < (layer + 1)->size(); ++neuron) {
            weighted_error = std::inner_product(
                errors.begin(),
                errors.end(),
                (layer + 1)->neurons[neuron].weights_to_next_layer.begin(),
                0.0);
            error = weighted_error * transfer_fn((layer + 1)->neurons[neuron].transfer_value);
            (layer + 1)->neurons[neuron].error = error;
            new_errors[neuron] = error;
        }
        errors = new_errors;
    }
    return cost;
}

void NeuralNetwork::update_weights(Point& datapoint, double lr, bool normalize_lr) {
    unsigned short int neuron, weight;
    double neuron_error, rate;
    if (normalize_lr)
        rate = lr / (1 + lr * std::accumulate(datapoint.x.begin(), datapoint.x.end(), 0.0));
    else
        rate = lr;
    vector<double> inputs(datapoint.x);
    for (vector<Layer>::iterator layer = layers.begin(); layer != layers.end() - 1; ++layer) {
        for (weight = 0; weight < (layer + 1)->size(); ++weight) {
            neuron_error = (layer + 1)->neurons[weight].error;
            for (neuron = 0; neuron < layer->size(); ++neuron)
                layer->neurons[neuron].weights_to_next_layer[weight] += rate*neuron_error*inputs[neuron];
        }
        inputs = vector<double>((layer + 1)->size());
        for (neuron = 0; neuron < (layer + 1)->size(); ++neuron)
            inputs[neuron] = (layer + 1)->neurons[neuron].transfer_value;
    }
}

double NeuralNetwork::predict(list<Point>& dataset, string transfer_function) {
    std::tuple<double (*)(double), double (*)(double)> transfer = get_transfer_methods(transfer_function);
    double (*transfer_fn)(double) = std::get<0>(transfer),
           (*transfer_derivative)(double) = std::get<1>(transfer);
    double total_cost = 0.0;
    unsigned int neuron, i = 1, total_correct = 0;
    map<int, int> class_counts;
    vector<double> prediction, diff(layers.back().size());
    for (list<Point>::iterator datapoint = dataset.begin(); datapoint != dataset.end(); ++datapoint, ++i) {
        cout << "ITERATION: " << i << ", ";
        cout << "Y = " << datapoint->y << ", ";
        prediction = forward_propagate(*datapoint, transfer_function);
        for (neuron = 0; neuron < layers.back().size(); ++neuron) {
            total_cost += layers.back().neurons[neuron].error;
            diff[neuron] = prediction[neuron] - datapoint->y[neuron];
        }
        cout << "Z = " << prediction;
        if (transfer_function != "none"
                && transfer_function != "tanh_regression"
                && transfer_function != "sigmoid_regression"
                && transfer_function != "relu_regression") {
            classify(prediction);
            cout << ", PREDICT:  " << prediction;
        }
        cout << std::flush;
        cout << endl;
        if (transfer_function != "none"
                && transfer_function != "tanh_regression"
                && transfer_function != "sigmoid_regression"
                && transfer_function != "relu_regression") {
            total_correct += (prediction == datapoint->y);
            for (unsigned short int i = 0; i < layers.back().size(); ++i) {
                if (class_counts.find(prediction[i]) != class_counts.end())
                    class_counts[prediction[i]] += prediction[i];
                else
                    class_counts[prediction[i]] = 1;
            }
        }
        else
            total_correct += abs(prediction[0] - datapoint->y[0]) <= .01;
    }
    cout << "TOTAL CORRECT: " << total_correct << ", " << 100.0*total_correct/dataset.size() << "%" << endl;
    if (transfer_function != "none"
            && transfer_function != "tanh_regression"
            && transfer_function != "sigmoid_regression"
            && transfer_function != "relu_regression") {
        cout << "PREDICTION COUNTS: " << endl;
        for (const auto& m : class_counts)
            cout << m.first << ": " << m.second << endl;
    }
    return 1.0*total_cost / dataset.size();
}


std::ostream& operator <<(std::ostream& out, const NeuralNetwork& n) {
    for (unsigned int i = 0; i < n.layers.size(); ++i)
        out << "Layer " << i + 1 << ": " << n.layers[i].size() << " Neurons, "
            << n.layers[i].neurons.front().weights_to_next_layer.size() << " Weights to Next Layer"
            << endl << n.layers[i] << endl << endl;
    return out;
}
