#!/bin/bash

# Script pour tester le module wq_insert_exec avec différentes configurations
# Usage: ./start.sh [module_path]

set -e  # Exit on any error

MODULE_PATH="$1"
MODULE_NAME=$(basename "${MODULE_PATH}" .ko)
TOTAL_ITERATIONS=100


# Vérifier que le module existe
if [[ ! -f "${MODULE_PATH}" ]]; then
    echo "Erreur: Module ${MODULE_PATH} introuvable!"
    echo "Usage: $0 [chemin_vers_module.ko]"
    exit 1
fi

# vérifier si le nombre d'itérations est passé en argument
if [[ -n "$2" ]]; then
    if [[ "$2" =~ ^[0-9]+$ ]]; then
        TOTAL_ITERATIONS="$2"
    else
        echo "Erreur: Le nombre d'itérations doit être un entier positif."
        exit 1
    fi
fi

xp_wq_insert_exec() {
    local high_affinity="$1"
    local unbound_or_bounded="$2"
    local iteration="$3"

    
    # Insérer le module avec les paramètres
    sudo insmod "${MODULE_PATH}" \
        high_affinity="${high_affinity}" \
        unbound_or_bounded="${unbound_or_bounded}" \
        iteration="${iteration}" || {
        echo "Erreur lors de l'insertion du module avec les paramètres: high_affinity=${high_affinity}, unbound_or_bounded=${unbound_or_bounded}, iterations=${iteration}"
        return 1
    }
    sudo rmmod "${MODULE_NAME}" || {
        echo "Erreur lors de la suppression du module ${MODULE_NAME}"
        return 1
    }
}

xp_wq_exec_pred() {
    local high_affinity="$1"
    local unbound_or_bounded="$2"
    local n_op_matrix="$3"

    # Insérer le module avec les paramètres
    sudo insmod "${MODULE_PATH}" \
        high_affinity="${high_affinity}" \
        unbound_or_bounded="${unbound_or_bounded}" \
        n_op_matrix="${n_op_matrix}" || {
        echo "Erreur lors de l'insertion du module avec les paramètres: high_affinity=${high_affinity}, unbound_or_bounded=${unbound_or_bounded}, n_op_matrix=${n_op_matrix}"
        return 1
    }
    sudo rmmod "${MODULE_NAME}" || {
        echo "Erreur lors de la suppression du module ${MODULE_NAME}"
        return 1
    }
}

# Fonction principale
main_xp_wq_insert_exec() {
    echo "Démarrage de xp_wq_insert_exec avec ${MODULE_PATH}..."
    echo "============================================"
    
    
    # Vérifier les permissions sudo
    if ! sudo -n true 2>/dev/null; then
        echo "Ce script nécessite des privilèges sudo pour insérer/supprimer les modules."
        echo "Veuillez entrer votre mot de passe sudo:"
        sudo true
    fi
    
    # Tableaux des configurations
    iterations=(1000 10000 100000)
    high_affinity_values=(0 1)
    unbound_or_bounded_values=(0 1)
    
    for ((i = 0; i < TOTAL_ITERATIONS; i++)); do
        echo "Exécution de l'itération $((i + 1)) sur $TOTAL_ITERATIONS..."
        for high_affinity in "${high_affinity_values[@]}"; do
            for unbound_or_bounded in "${unbound_or_bounded_values[@]}"; do
                for iteration in "${iterations[@]}"; do
                    xp_wq_insert_exec "$high_affinity" "$unbound_or_bounded" "$iteration"
                    if [[ $? -ne 0 ]]; then
                        echo "Échec de l'exécution avec high_affinity=${high_affinity}, unbound_or_bounded=${unbound_or_bounded}, iteration=${iteration}"
                        exit 1
                    fi
                done
            done
        done
    done

    echo "Tous les tests ont été effectués avec succès."
    echo "============================================"
}

# Fonction principale
main_xp_wq_exec_pred() {
    echo "Démarrage de xp_wq_exec_pred avec ${MODULE_PATH}..."
    echo "============================================"
    
    
    # Vérifier les permissions sudo
    if ! sudo -n true 2>/dev/null; then
        echo "Ce script nécessite des privilèges sudo pour insérer/supprimer les modules."
        echo "Veuillez entrer votre mot de passe sudo:"
        sudo true
    fi
    
    # Tableaux des configurations
    high_affinity_values=(0 1)
    unbound_or_bounded_values=(0 1)
    n_op_matrix_values=(1000 2000 3000 4000 5000)
    
    iteration=1000

    for ((i = 0; i < TOTAL_ITERATIONS; i++)); do
        echo "Exécution de l'itération $((i + 1)) sur $TOTAL_ITERATIONS..."
        for high_affinity in "${high_affinity_values[@]}"; do
            for unbound_or_bounded in "${unbound_or_bounded_values[@]}"; do
                for ((i=0;i<$iteration;i++)); do
                    for n_op_matrix in "${n_op_matrix_values[@]}"; do
                        xp_wq_exec_pred "$high_affinity" "$unbound_or_bounded" "$n_op_matrix"
                        if [[ $? -ne 0 ]]; then
                            echo "Échec de l'exécution avec high_affinity=${high_affinity}, unbound_or_bounded=${unbound_or_bounded}, n_op_matrix=${n_op_matrix}"
                            exit 1
                        fi
                    done
                done
            done
        done
    done

    echo "Tous les tests ont été effectués avec succès."
    echo "============================================"
}

# Exécuter le script principal
main "$@"
