#!/bin/bash

# Script pour tester le module wq_insert_exec avec différentes configurations
# Usage: ./start.sh [module_path]

set -e  # Exit on any error

MODULE_PATH="$1"
MODULE_NAME=$(basename "${MODULE_PATH}" .ko)


# Vérifier que le module existe
if [[ ! -f "${MODULE_PATH}" ]]; then
    echo "Erreur: Module ${MODULE_PATH} introuvable!"
    echo "Usage: $0 [chemin_vers_module.ko]"
    exit 1
fi

# Fonction principale
main() {
    echo "Démarrage des tests du module ${MODULE_PATH}..."
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
    
    # Boucles pour toutes les combinaisons
    for high_affinity in "${high_affinity_values[@]}"; do
        for unbound_or_bounded in "${unbound_or_bounded_values[@]}"; do
            for iteration in "${iterations[@]}"; do
                
                sudo insmod  "${MODULE_PATH}" \
                    high_affinity="${high_affinity}" \
                    unbound_or_bounded="${unbound_or_bounded}" \
                    iterations="${iteration}" || {
                    echo "Erreur lors de l'insertion du module avec les paramètres: high_affinity=${high_affinity}, unbound_or_bounded=${unbound_or_bounded}, iterations=${iteration}"
                    continue
                }
                sleep 2
                sudo rmmod "${MODULE_NAME}" || {
                    echo "Erreur lors de la suppression du module ${MODULE_NAME}"
                    continue
                }
            done
        done
    done
    
    echo "Tous les tests ont été effectués avec succès."
    echo "============================================"
}

# Exécuter le script principal
main "$@"
